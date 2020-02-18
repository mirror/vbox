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





section VGAROM progbits vstart=0x0 align=1 ; size=0x8ea class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x8ea -> off=0x22 cb=000000000000053e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e3h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    call 008d6h                               ; e8 fd 07                    ; 0xc00d6 vgarom.asm:190
    jmp short 000e7h                          ; eb 0c                       ; 0xc00d9 vgarom.asm:191
    push ES                                   ; 06                          ; 0xc00db vgarom.asm:195
    push DS                                   ; 1e                          ; 0xc00dc vgarom.asm:196
    pushaw                                    ; 60                          ; 0xc00dd vgarom.asm:97
    push CS                                   ; 0e                          ; 0xc00de vgarom.asm:200
    pop DS                                    ; 1f                          ; 0xc00df vgarom.asm:201
    cld                                       ; fc                          ; 0xc00e0 vgarom.asm:202
    call 03670h                               ; e8 8c 35                    ; 0xc00e1 vgarom.asm:203
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
  ; disGetNextSymbol 0xc0560 LB 0x38a -> off=0x0 cb=0000000000000007 uValue=00000000000c0560 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0560 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0560 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0562 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0563 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0565 vberom.asm:72
    retn                                      ; c3                          ; 0xc0566 vberom.asm:73
  ; disGetNextSymbol 0xc0567 LB 0x383 -> off=0x0 cb=0000000000000040 uValue=00000000000c0567 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05a7 LB 0x343 -> off=0x0 cb=0000000000000026 uValue=00000000000c05a7 '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc05cd LB 0x31d -> off=0x0 cb=0000000000000026 uValue=00000000000c05cd 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc05f3 LB 0x2f7 -> off=0x0 cb=0000000000000026 uValue=00000000000c05f3 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc0619 LB 0x2d1 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0619 '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06c2 LB 0x228 -> off=0x0 cb=00000000000000ed uValue=00000000000c06c2 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07af LB 0x13b -> off=0x0 cb=0000000000000013 uValue=00000000000c07af '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07c2 LB 0x128 -> off=0x0 cb=0000000000000025 uValue=00000000000c07c2 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc07e7 LB 0x103 -> off=0x0 cb=000000000000002d uValue=00000000000c07e7 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc0814 LB 0xd6 -> off=0x0 cb=0000000000000034 uValue=00000000000c0814 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0848 LB 0xa2 -> off=0x0 cb=0000000000000037 uValue=00000000000c0848 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc087f LB 0x6b -> off=0x0 cb=0000000000000057 uValue=00000000000c087f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc087f LB 0x57
    test bl, bl                               ; 84 db                       ; 0xc087f vberom.asm:683
    je short 00892h                           ; 74 0f                       ; 0xc0881 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0883 vberom.asm:685
    je short 008b2h                           ; 74 2a                       ; 0xc0886 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0888 vberom.asm:687
    jbe short 008d2h                          ; 76 45                       ; 0xc088b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc088d vberom.asm:689
    jne short 008ceh                          ; 75 3c                       ; 0xc0890 vberom.asm:690
    pushaw                                    ; 60                          ; 0xc0892 vberom.asm:133
    push DS                                   ; 1e                          ; 0xc0893 vberom.asm:696
    push ES                                   ; 06                          ; 0xc0894 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc0895 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc0896 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0898 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc089b vberom.asm:701
    inc dx                                    ; 42                          ; 0xc089c vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc089d vberom.asm:703
    lodsw                                     ; ad                          ; 0xc089f vberom.asm:714
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc08a0 vberom.asm:715
    lodsw                                     ; ad                          ; 0xc08a2 vberom.asm:716
    out DX, AL                                ; ee                          ; 0xc08a3 vberom.asm:717
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc08a4 vberom.asm:718
    out DX, AL                                ; ee                          ; 0xc08a6 vberom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc08a7 vberom.asm:720
    out DX, AL                                ; ee                          ; 0xc08a9 vberom.asm:721
    loop 0089fh                               ; e2 f3                       ; 0xc08aa vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08ac vberom.asm:724
    popaw                                     ; 61                          ; 0xc08ad vberom.asm:152
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08ae vberom.asm:727
    retn                                      ; c3                          ; 0xc08b1 vberom.asm:728
    pushaw                                    ; 60                          ; 0xc08b2 vberom.asm:133
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08b3 vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08b5 vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08b8 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08b9 vberom.asm:735
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db                     ; 0xc08bc vberom.asm:746
    in AL, DX                                 ; ec                          ; 0xc08be vberom.asm:748
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc08bf vberom.asm:749
    in AL, DX                                 ; ec                          ; 0xc08c1 vberom.asm:750
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc08c2 vberom.asm:751
    in AL, DX                                 ; ec                          ; 0xc08c4 vberom.asm:752
    stosw                                     ; ab                          ; 0xc08c5 vberom.asm:753
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc08c6 vberom.asm:754
    stosw                                     ; ab                          ; 0xc08c8 vberom.asm:755
    loop 008beh                               ; e2 f3                       ; 0xc08c9 vberom.asm:757
    popaw                                     ; 61                          ; 0xc08cb vberom.asm:152
    jmp short 008aeh                          ; eb e0                       ; 0xc08cc vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08ce vberom.asm:762
    retn                                      ; c3                          ; 0xc08d1 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08d2 vberom.asm:765
    retn                                      ; c3                          ; 0xc08d5 vberom.asm:766
  ; disGetNextSymbol 0xc08d6 LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c08d6 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08d6 LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc08d6 vberom.asm:780
    jne short 008e6h                          ; 75 0c                       ; 0xc08d8 vberom.asm:781
    push CS                                   ; 0e                          ; 0xc08da vberom.asm:782
    pop ES                                    ; 07                          ; 0xc08db vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc08dc vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08df vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08e2 vberom.asm:786
    retn                                      ; c3                          ; 0xc08e5 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08e6 vberom.asm:789
    retn                                      ; c3                          ; 0xc08e9 vberom.asm:790

  ; Padding 0x96 bytes at 0xc08ea
  times 150 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x38ef class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x38ef -> off=0x0 cb=000000000000001b uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1b
    push dx                                   ; 52                          ; 0xc0980 vgabios.c:88
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    mov dx, bx                                ; 89 da                       ; 0xc0984
    mov bl, al                                ; 88 c3                       ; 0xc0986 vgabios.c:92
    xor bh, bh                                ; 30 ff                       ; 0xc0988
    sal bx, 002h                              ; c1 e3 02                    ; 0xc098a
    xor ax, ax                                ; 31 c0                       ; 0xc098d
    mov es, ax                                ; 8e c0                       ; 0xc098f
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0991
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0994
    pop bp                                    ; 5d                          ; 0xc0998 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc0999
    retn                                      ; c3                          ; 0xc099a
  ; disGetNextSymbol 0xc099b LB 0x38d4 -> off=0x0 cb=000000000000001c uValue=00000000000c099b 'init_vga_card'
init_vga_card:                               ; 0xc099b LB 0x1c
    push bp                                   ; 55                          ; 0xc099b vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc099c
    push dx                                   ; 52                          ; 0xc099e
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc099f vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a1
    out DX, AL                                ; ee                          ; 0xc09a4
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a5 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a7
    out DX, AL                                ; ee                          ; 0xc09aa
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09ab vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ad
    out DX, AL                                ; ee                          ; 0xc09b0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b1 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc09b4
    pop bp                                    ; 5d                          ; 0xc09b5
    retn                                      ; c3                          ; 0xc09b6
  ; disGetNextSymbol 0xc09b7 LB 0x38b8 -> off=0x0 cb=0000000000000032 uValue=00000000000c09b7 'init_bios_area'
init_bios_area:                              ; 0xc09b7 LB 0x32
    push bx                                   ; 53                          ; 0xc09b7 vgabios.c:165
    push bp                                   ; 55                          ; 0xc09b8
    mov bp, sp                                ; 89 e5                       ; 0xc09b9
    xor bx, bx                                ; 31 db                       ; 0xc09bb vgabios.c:169
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09bd
    mov es, ax                                ; 8e c0                       ; 0xc09c0
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c2 vgabios.c:172
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c6
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c8
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09ca
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09ce vgabios.c:176
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d4 vgabios.c:178
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09db vgabios.c:182
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e1 vgabios.c:184
    pop bp                                    ; 5d                          ; 0xc09e6 vgabios.c:185
    pop bx                                    ; 5b                          ; 0xc09e7
    retn                                      ; c3                          ; 0xc09e8
  ; disGetNextSymbol 0xc09e9 LB 0x3886 -> off=0x0 cb=0000000000000031 uValue=00000000000c09e9 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09e9 LB 0x31
    inc bp                                    ; 45                          ; 0xc09e9 vgabios.c:225
    push bp                                   ; 55                          ; 0xc09ea
    mov bp, sp                                ; 89 e5                       ; 0xc09eb
    call 0099bh                               ; e8 ab ff                    ; 0xc09ed vgabios.c:227
    call 009b7h                               ; e8 c4 ff                    ; 0xc09f0 vgabios.c:228
    call 03c00h                               ; e8 0a 32                    ; 0xc09f3 vgabios.c:230
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc09f6 vgabios.c:232
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc09f9
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09fc
    call 00980h                               ; e8 7e ff                    ; 0xc09ff
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc0a02 vgabios.c:233
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a05
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a08
    call 00980h                               ; e8 72 ff                    ; 0xc0a0b
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a0e vgabios.c:259
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a11
    int 010h                                  ; cd 10                       ; 0xc0a13
    mov sp, bp                                ; 89 ec                       ; 0xc0a15 vgabios.c:262
    pop bp                                    ; 5d                          ; 0xc0a17
    dec bp                                    ; 4d                          ; 0xc0a18
    retf                                      ; cb                          ; 0xc0a19
  ; disGetNextSymbol 0xc0a1a LB 0x3855 -> off=0x0 cb=0000000000000040 uValue=00000000000c0a1a 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a1a LB 0x40
    push si                                   ; 56                          ; 0xc0a1a vgabios.c:331
    push di                                   ; 57                          ; 0xc0a1b
    push bp                                   ; 55                          ; 0xc0a1c
    mov bp, sp                                ; 89 e5                       ; 0xc0a1d
    mov si, dx                                ; 89 d6                       ; 0xc0a1f
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a21 vgabios.c:333
    jbe short 00a33h                          ; 76 0e                       ; 0xc0a23
    push SS                                   ; 16                          ; 0xc0a25 vgabios.c:334
    pop ES                                    ; 07                          ; 0xc0a26
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a27
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a2c vgabios.c:335
    jmp short 00a56h                          ; eb 23                       ; 0xc0a31 vgabios.c:336
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a33 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a36
    mov es, dx                                ; 8e c2                       ; 0xc0a39
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a3b
    push SS                                   ; 16                          ; 0xc0a3e vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a3f
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a40
    xor ah, ah                                ; 30 e4                       ; 0xc0a43 vgabios.c:339
    mov si, ax                                ; 89 c6                       ; 0xc0a45
    add si, ax                                ; 01 c6                       ; 0xc0a47
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a49
    mov es, dx                                ; 8e c2                       ; 0xc0a4c vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0a4e
    push SS                                   ; 16                          ; 0xc0a51 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0a53
    pop bp                                    ; 5d                          ; 0xc0a56 vgabios.c:341
    pop di                                    ; 5f                          ; 0xc0a57
    pop si                                    ; 5e                          ; 0xc0a58
    retn                                      ; c3                          ; 0xc0a59
  ; disGetNextSymbol 0xc0a5a LB 0x3815 -> off=0x0 cb=000000000000005e uValue=00000000000c0a5a 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a5a LB 0x5e
    push bp                                   ; 55                          ; 0xc0a5a vgabios.c:344
    mov bp, sp                                ; 89 e5                       ; 0xc0a5b
    push si                                   ; 56                          ; 0xc0a5d
    push di                                   ; 57                          ; 0xc0a5e
    push ax                                   ; 50                          ; 0xc0a5f
    push ax                                   ; 50                          ; 0xc0a60
    push dx                                   ; 52                          ; 0xc0a61
    push bx                                   ; 53                          ; 0xc0a62
    mov bl, cl                                ; 88 cb                       ; 0xc0a63
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a65 vgabios.c:346
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a6a vgabios.c:348
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a6d
    je short 00aach                           ; 74 39                       ; 0xc0a71
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0a73 vgabios.c:349
    xor ch, ch                                ; 30 ed                       ; 0xc0a76
    mov dx, ss                                ; 8c d2                       ; 0xc0a78
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a7a
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a7d
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a80
    push DS                                   ; 1e                          ; 0xc0a83
    mov ds, dx                                ; 8e da                       ; 0xc0a84
    rep cmpsb                                 ; f3 a6                       ; 0xc0a86
    pop DS                                    ; 1f                          ; 0xc0a88
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a89
    je short 00a90h                           ; 74 02                       ; 0xc0a8c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a8e
    test ax, ax                               ; 85 c0                       ; 0xc0a90
    jne short 00aa0h                          ; 75 0c                       ; 0xc0a92
    mov al, bl                                ; 88 d8                       ; 0xc0a94 vgabios.c:350
    xor ah, ah                                ; 30 e4                       ; 0xc0a96
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a98
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a9b
    jmp short 00aach                          ; eb 0c                       ; 0xc0a9e vgabios.c:351
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0aa0 vgabios.c:353
    xor ah, ah                                ; 30 e4                       ; 0xc0aa3
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0aa5
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0aa8 vgabios.c:354
    jmp short 00a6ah                          ; eb be                       ; 0xc0aaa vgabios.c:355
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0aac vgabios.c:357
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0aaf
    pop di                                    ; 5f                          ; 0xc0ab2
    pop si                                    ; 5e                          ; 0xc0ab3
    pop bp                                    ; 5d                          ; 0xc0ab4
    retn 00004h                               ; c2 04 00                    ; 0xc0ab5
  ; disGetNextSymbol 0xc0ab8 LB 0x37b7 -> off=0x0 cb=0000000000000046 uValue=00000000000c0ab8 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0ab8 LB 0x46
    push bp                                   ; 55                          ; 0xc0ab8 vgabios.c:359
    mov bp, sp                                ; 89 e5                       ; 0xc0ab9
    push si                                   ; 56                          ; 0xc0abb
    push di                                   ; 57                          ; 0xc0abc
    push ax                                   ; 50                          ; 0xc0abd
    push ax                                   ; 50                          ; 0xc0abe
    mov si, ax                                ; 89 c6                       ; 0xc0abf
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0ac1
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0ac4
    mov bx, cx                                ; 89 cb                       ; 0xc0ac7
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0ac9 vgabios.c:366
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0acc
    out DX, ax                                ; ef                          ; 0xc0acf
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0ad0 vgabios.c:368
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0ad3
    je short 00aeeh                           ; 74 15                       ; 0xc0ad7
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0ad9 vgabios.c:369
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0adc
    not al                                    ; f6 d0                       ; 0xc0adf
    mov di, bx                                ; 89 df                       ; 0xc0ae1
    inc bx                                    ; 43                          ; 0xc0ae3
    push SS                                   ; 16                          ; 0xc0ae4
    pop ES                                    ; 07                          ; 0xc0ae5
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ae6
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0ae9 vgabios.c:370
    jmp short 00ad0h                          ; eb e2                       ; 0xc0aec vgabios.c:371
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0aee vgabios.c:374
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0af1
    out DX, ax                                ; ef                          ; 0xc0af4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0af5 vgabios.c:375
    pop di                                    ; 5f                          ; 0xc0af8
    pop si                                    ; 5e                          ; 0xc0af9
    pop bp                                    ; 5d                          ; 0xc0afa
    retn 00002h                               ; c2 02 00                    ; 0xc0afb
  ; disGetNextSymbol 0xc0afe LB 0x3771 -> off=0x0 cb=000000000000002f uValue=00000000000c0afe 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0afe LB 0x2f
    push si                                   ; 56                          ; 0xc0afe vgabios.c:377
    push bp                                   ; 55                          ; 0xc0aff
    mov bp, sp                                ; 89 e5                       ; 0xc0b00
    mov ch, al                                ; 88 c5                       ; 0xc0b02
    mov al, dl                                ; 88 d0                       ; 0xc0b04
    xor ah, ah                                ; 30 e4                       ; 0xc0b06 vgabios.c:381
    mul bx                                    ; f7 e3                       ; 0xc0b08
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0b0a
    xor bh, bh                                ; 30 ff                       ; 0xc0b0d
    mul bx                                    ; f7 e3                       ; 0xc0b0f
    mov bl, ch                                ; 88 eb                       ; 0xc0b11
    add bx, ax                                ; 01 c3                       ; 0xc0b13
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b15 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b18
    mov es, ax                                ; 8e c0                       ; 0xc0b1b
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b1d
    mov al, cl                                ; 88 c8                       ; 0xc0b20 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0b22
    mul si                                    ; f7 e6                       ; 0xc0b24
    add ax, bx                                ; 01 d8                       ; 0xc0b26
    pop bp                                    ; 5d                          ; 0xc0b28 vgabios.c:385
    pop si                                    ; 5e                          ; 0xc0b29
    retn 00002h                               ; c2 02 00                    ; 0xc0b2a
  ; disGetNextSymbol 0xc0b2d LB 0x3742 -> off=0x0 cb=0000000000000040 uValue=00000000000c0b2d 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b2d LB 0x40
    push bp                                   ; 55                          ; 0xc0b2d vgabios.c:387
    mov bp, sp                                ; 89 e5                       ; 0xc0b2e
    push cx                                   ; 51                          ; 0xc0b30
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0b31
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0b34 vgabios.c:391
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0b37
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0b3b
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0b3e
    mov bx, ax                                ; 89 c3                       ; 0xc0b41
    mov ax, dx                                ; 89 d0                       ; 0xc0b43
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b45
    call 00ab8h                               ; e8 6d ff                    ; 0xc0b48
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0b4b vgabios.c:394
    push 00100h                               ; 68 00 01                    ; 0xc0b4e
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b51 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b54
    mov es, ax                                ; 8e c0                       ; 0xc0b56
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b58
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b5b
    xor cx, cx                                ; 31 c9                       ; 0xc0b5f vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0b61
    call 00a5ah                               ; e8 f3 fe                    ; 0xc0b64
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0b67 vgabios.c:395
    pop cx                                    ; 59                          ; 0xc0b6a
    pop bp                                    ; 5d                          ; 0xc0b6b
    retn                                      ; c3                          ; 0xc0b6c
  ; disGetNextSymbol 0xc0b6d LB 0x3702 -> off=0x0 cb=0000000000000024 uValue=00000000000c0b6d 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b6d LB 0x24
    enter 00002h, 000h                        ; c8 02 00 00                 ; 0xc0b6d vgabios.c:397
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0b71
    mov al, dl                                ; 88 d0                       ; 0xc0b74 vgabios.c:401
    xor ah, ah                                ; 30 e4                       ; 0xc0b76
    mul bx                                    ; f7 e3                       ; 0xc0b78
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0b7a
    xor dh, dh                                ; 30 f6                       ; 0xc0b7d
    mul dx                                    ; f7 e2                       ; 0xc0b7f
    mov dx, ax                                ; 89 c2                       ; 0xc0b81
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0b83
    xor ah, ah                                ; 30 e4                       ; 0xc0b86
    add ax, dx                                ; 01 d0                       ; 0xc0b88
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0b8a vgabios.c:402
    leave                                     ; c9                          ; 0xc0b8d vgabios.c:404
    retn 00002h                               ; c2 02 00                    ; 0xc0b8e
  ; disGetNextSymbol 0xc0b91 LB 0x36de -> off=0x0 cb=000000000000004b uValue=00000000000c0b91 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b91 LB 0x4b
    push si                                   ; 56                          ; 0xc0b91 vgabios.c:406
    push di                                   ; 57                          ; 0xc0b92
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0b93
    mov si, ax                                ; 89 c6                       ; 0xc0b97
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0b99
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0b9c
    mov bx, cx                                ; 89 cb                       ; 0xc0b9f
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0ba1 vgabios.c:412
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0ba4
    je short 00bd6h                           ; 74 2c                       ; 0xc0ba8
    xor dh, dh                                ; 30 f6                       ; 0xc0baa vgabios.c:413
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0bac vgabios.c:414
    xor ax, ax                                ; 31 c0                       ; 0xc0bae vgabios.c:415
    jmp short 00bb7h                          ; eb 05                       ; 0xc0bb0
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0bb2
    jnl short 00bcbh                          ; 7d 14                       ; 0xc0bb5
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0bb7 vgabios.c:416
    mov di, si                                ; 89 f7                       ; 0xc0bba
    add di, ax                                ; 01 c7                       ; 0xc0bbc
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0bbe
    je short 00bc6h                           ; 74 02                       ; 0xc0bc2
    or dh, dl                                 ; 08 d6                       ; 0xc0bc4 vgabios.c:417
    shr dl, 1                                 ; d0 ea                       ; 0xc0bc6 vgabios.c:418
    inc ax                                    ; 40                          ; 0xc0bc8 vgabios.c:419
    jmp short 00bb2h                          ; eb e7                       ; 0xc0bc9
    mov di, bx                                ; 89 df                       ; 0xc0bcb vgabios.c:420
    inc bx                                    ; 43                          ; 0xc0bcd
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0bce
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bd1 vgabios.c:421
    jmp short 00ba1h                          ; eb cb                       ; 0xc0bd4 vgabios.c:422
    leave                                     ; c9                          ; 0xc0bd6 vgabios.c:423
    pop di                                    ; 5f                          ; 0xc0bd7
    pop si                                    ; 5e                          ; 0xc0bd8
    retn 00002h                               ; c2 02 00                    ; 0xc0bd9
  ; disGetNextSymbol 0xc0bdc LB 0x3693 -> off=0x0 cb=0000000000000045 uValue=00000000000c0bdc 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0bdc LB 0x45
    push bp                                   ; 55                          ; 0xc0bdc vgabios.c:425
    mov bp, sp                                ; 89 e5                       ; 0xc0bdd
    push cx                                   ; 51                          ; 0xc0bdf
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0be0
    mov cx, ax                                ; 89 c1                       ; 0xc0be3
    mov ax, dx                                ; 89 d0                       ; 0xc0be5
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0be7 vgabios.c:429
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0bea
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bee
    mov bx, cx                                ; 89 cb                       ; 0xc0bf1
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0bf3
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0bf6
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bf9
    call 00b91h                               ; e8 92 ff                    ; 0xc0bfc
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bff vgabios.c:432
    push 00100h                               ; 68 00 01                    ; 0xc0c02
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c05 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0c08
    mov es, ax                                ; 8e c0                       ; 0xc0c0a
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c0c
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c0f
    xor cx, cx                                ; 31 c9                       ; 0xc0c13 vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0c15
    call 00a5ah                               ; e8 3f fe                    ; 0xc0c18
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0c1b vgabios.c:433
    pop cx                                    ; 59                          ; 0xc0c1e
    pop bp                                    ; 5d                          ; 0xc0c1f
    retn                                      ; c3                          ; 0xc0c20
  ; disGetNextSymbol 0xc0c21 LB 0x364e -> off=0x0 cb=0000000000000035 uValue=00000000000c0c21 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c21 LB 0x35
    push bp                                   ; 55                          ; 0xc0c21 vgabios.c:435
    mov bp, sp                                ; 89 e5                       ; 0xc0c22
    push bx                                   ; 53                          ; 0xc0c24
    push cx                                   ; 51                          ; 0xc0c25
    mov bx, ax                                ; 89 c3                       ; 0xc0c26
    mov es, dx                                ; 8e c2                       ; 0xc0c28
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c2a vgabios.c:441
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c2d vgabios.c:442
    xor dl, dl                                ; 30 d2                       ; 0xc0c2f vgabios.c:443
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c31 vgabios.c:444
    xchg ah, al                               ; 86 c4                       ; 0xc0c34
    xor bx, bx                                ; 31 db                       ; 0xc0c36 vgabios.c:446
    jmp short 00c3fh                          ; eb 05                       ; 0xc0c38
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c3a
    jnl short 00c4dh                          ; 7d 0e                       ; 0xc0c3d
    test ax, cx                               ; 85 c8                       ; 0xc0c3f vgabios.c:447
    je short 00c45h                           ; 74 02                       ; 0xc0c41
    or dl, dh                                 ; 08 f2                       ; 0xc0c43 vgabios.c:448
    shr dh, 1                                 ; d0 ee                       ; 0xc0c45 vgabios.c:449
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0c47 vgabios.c:450
    inc bx                                    ; 43                          ; 0xc0c4a vgabios.c:451
    jmp short 00c3ah                          ; eb ed                       ; 0xc0c4b
    mov al, dl                                ; 88 d0                       ; 0xc0c4d vgabios.c:453
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c4f
    pop cx                                    ; 59                          ; 0xc0c52
    pop bx                                    ; 5b                          ; 0xc0c53
    pop bp                                    ; 5d                          ; 0xc0c54
    retn                                      ; c3                          ; 0xc0c55
  ; disGetNextSymbol 0xc0c56 LB 0x3619 -> off=0x0 cb=0000000000000084 uValue=00000000000c0c56 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c56 LB 0x84
    push bp                                   ; 55                          ; 0xc0c56 vgabios.c:455
    mov bp, sp                                ; 89 e5                       ; 0xc0c57
    push cx                                   ; 51                          ; 0xc0c59
    push si                                   ; 56                          ; 0xc0c5a
    push di                                   ; 57                          ; 0xc0c5b
    push ax                                   ; 50                          ; 0xc0c5c
    mov si, dx                                ; 89 d6                       ; 0xc0c5d
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c5f vgabios.c:463
    je short 00c9eh                           ; 74 3a                       ; 0xc0c62
    mov bx, ax                                ; 89 c3                       ; 0xc0c64 vgabios.c:465
    add bx, ax                                ; 01 c3                       ; 0xc0c66
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c68
    xor cx, cx                                ; 31 c9                       ; 0xc0c6d vgabios.c:467
    jmp short 00c76h                          ; eb 05                       ; 0xc0c6f
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c71
    jnl short 00cd2h                          ; 7d 5c                       ; 0xc0c74
    mov ax, bx                                ; 89 d8                       ; 0xc0c76 vgabios.c:468
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c78
    call 00c21h                               ; e8 a3 ff                    ; 0xc0c7b
    mov di, si                                ; 89 f7                       ; 0xc0c7e
    inc si                                    ; 46                          ; 0xc0c80
    push SS                                   ; 16                          ; 0xc0c81
    pop ES                                    ; 07                          ; 0xc0c82
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c83
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c86 vgabios.c:469
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c8a
    call 00c21h                               ; e8 91 ff                    ; 0xc0c8d
    mov di, si                                ; 89 f7                       ; 0xc0c90
    inc si                                    ; 46                          ; 0xc0c92
    push SS                                   ; 16                          ; 0xc0c93
    pop ES                                    ; 07                          ; 0xc0c94
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c95
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0c98 vgabios.c:470
    inc cx                                    ; 41                          ; 0xc0c9b vgabios.c:471
    jmp short 00c71h                          ; eb d3                       ; 0xc0c9c
    mov bx, ax                                ; 89 c3                       ; 0xc0c9e vgabios.c:473
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0ca0
    xor cx, cx                                ; 31 c9                       ; 0xc0ca5 vgabios.c:474
    jmp short 00caeh                          ; eb 05                       ; 0xc0ca7
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0ca9
    jnl short 00cd2h                          ; 7d 24                       ; 0xc0cac
    mov di, si                                ; 89 f7                       ; 0xc0cae vgabios.c:475
    inc si                                    ; 46                          ; 0xc0cb0
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cb1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0cb4
    push SS                                   ; 16                          ; 0xc0cb7
    pop ES                                    ; 07                          ; 0xc0cb8
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cb9
    mov di, si                                ; 89 f7                       ; 0xc0cbc vgabios.c:476
    inc si                                    ; 46                          ; 0xc0cbe
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cbf
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0cc2
    push SS                                   ; 16                          ; 0xc0cc7
    pop ES                                    ; 07                          ; 0xc0cc8
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cc9
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0ccc vgabios.c:477
    inc cx                                    ; 41                          ; 0xc0ccf vgabios.c:478
    jmp short 00ca9h                          ; eb d7                       ; 0xc0cd0
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0cd2 vgabios.c:480
    pop di                                    ; 5f                          ; 0xc0cd5
    pop si                                    ; 5e                          ; 0xc0cd6
    pop cx                                    ; 59                          ; 0xc0cd7
    pop bp                                    ; 5d                          ; 0xc0cd8
    retn                                      ; c3                          ; 0xc0cd9
  ; disGetNextSymbol 0xc0cda LB 0x3595 -> off=0x0 cb=000000000000001a uValue=00000000000c0cda 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0cda LB 0x1a
    push cx                                   ; 51                          ; 0xc0cda vgabios.c:482
    push bp                                   ; 55                          ; 0xc0cdb
    mov bp, sp                                ; 89 e5                       ; 0xc0cdc
    mov cl, al                                ; 88 c1                       ; 0xc0cde
    mov al, dl                                ; 88 d0                       ; 0xc0ce0
    xor ah, ah                                ; 30 e4                       ; 0xc0ce2 vgabios.c:487
    mul bx                                    ; f7 e3                       ; 0xc0ce4
    mov bx, ax                                ; 89 c3                       ; 0xc0ce6
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0ce8
    mov al, cl                                ; 88 c8                       ; 0xc0ceb
    xor ah, ah                                ; 30 e4                       ; 0xc0ced
    add ax, bx                                ; 01 d8                       ; 0xc0cef
    pop bp                                    ; 5d                          ; 0xc0cf1 vgabios.c:488
    pop cx                                    ; 59                          ; 0xc0cf2
    retn                                      ; c3                          ; 0xc0cf3
  ; disGetNextSymbol 0xc0cf4 LB 0x357b -> off=0x0 cb=0000000000000066 uValue=00000000000c0cf4 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0cf4 LB 0x66
    push bp                                   ; 55                          ; 0xc0cf4 vgabios.c:490
    mov bp, sp                                ; 89 e5                       ; 0xc0cf5
    push bx                                   ; 53                          ; 0xc0cf7
    push cx                                   ; 51                          ; 0xc0cf8
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0cf9
    mov bl, dl                                ; 88 d3                       ; 0xc0cfc vgabios.c:496
    xor bh, bh                                ; 30 ff                       ; 0xc0cfe
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d00
    call 00c56h                               ; e8 50 ff                    ; 0xc0d03
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d06 vgabios.c:499
    push 00080h                               ; 68 80 00                    ; 0xc0d08
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d0b vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d0e
    mov es, ax                                ; 8e c0                       ; 0xc0d10
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d12
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d15
    xor cx, cx                                ; 31 c9                       ; 0xc0d19 vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d1b
    call 00a5ah                               ; e8 39 fd                    ; 0xc0d1e
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d21
    test ah, 080h                             ; f6 c4 80                    ; 0xc0d24 vgabios.c:501
    jne short 00d50h                          ; 75 27                       ; 0xc0d27
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d29 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d2c
    mov es, ax                                ; 8e c0                       ; 0xc0d2e
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d30
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d33
    test dx, dx                               ; 85 d2                       ; 0xc0d37 vgabios.c:505
    jne short 00d3fh                          ; 75 04                       ; 0xc0d39
    test ax, ax                               ; 85 c0                       ; 0xc0d3b
    je short 00d50h                           ; 74 11                       ; 0xc0d3d
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d3f vgabios.c:506
    push 00080h                               ; 68 80 00                    ; 0xc0d41
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0d44
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d47
    call 00a5ah                               ; e8 0d fd                    ; 0xc0d4a
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d4d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d50 vgabios.c:509
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d53
    pop cx                                    ; 59                          ; 0xc0d56
    pop bx                                    ; 5b                          ; 0xc0d57
    pop bp                                    ; 5d                          ; 0xc0d58
    retn                                      ; c3                          ; 0xc0d59
  ; disGetNextSymbol 0xc0d5a LB 0x3515 -> off=0x0 cb=0000000000000130 uValue=00000000000c0d5a 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d5a LB 0x130
    push bp                                   ; 55                          ; 0xc0d5a vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0d5b
    push bx                                   ; 53                          ; 0xc0d5d
    push cx                                   ; 51                          ; 0xc0d5e
    push si                                   ; 56                          ; 0xc0d5f
    push di                                   ; 57                          ; 0xc0d60
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc0d61
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0d64
    mov si, dx                                ; 89 d6                       ; 0xc0d67
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d69 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d6c
    mov es, ax                                ; 8e c0                       ; 0xc0d6f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d71
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0d74 vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d77 vgabios.c:519
    call 035b3h                               ; e8 37 28                    ; 0xc0d79
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d7c
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d7f vgabios.c:520
    jne short 00d86h                          ; 75 03                       ; 0xc0d81
    jmp near 00e81h                           ; e9 fb 00                    ; 0xc0d83
    mov cl, byte [bp-00eh]                    ; 8a 4e f2                    ; 0xc0d86 vgabios.c:524
    xor ch, ch                                ; 30 ed                       ; 0xc0d89
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc0d8b
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0d8e
    mov ax, cx                                ; 89 c8                       ; 0xc0d91
    call 00a1ah                               ; e8 84 fc                    ; 0xc0d93
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc0d96 vgabios.c:525
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d99
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc0d9c vgabios.c:526
    xor al, al                                ; 30 c0                       ; 0xc0d9f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0da1
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc0da4
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc0da7
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0daa vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0dad
    mov es, ax                                ; 8e c0                       ; 0xc0db0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0db2
    xor ah, ah                                ; 30 e4                       ; 0xc0db5 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc0db7
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc0db8
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0dbb vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0dbe
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc0dc1 vgabios.c:48
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc0dc4 vgabios.c:532
    xor bh, bh                                ; 30 ff                       ; 0xc0dc7
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0dc9
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0dcc
    jne short 00e03h                          ; 75 30                       ; 0xc0dd1
    mov ax, di                                ; 89 f8                       ; 0xc0dd3 vgabios.c:534
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc0dd5
    add ax, ax                                ; 01 c0                       ; 0xc0dd8
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0dda
    inc ax                                    ; 40                          ; 0xc0ddc
    mul cx                                    ; f7 e1                       ; 0xc0ddd
    mov cx, ax                                ; 89 c1                       ; 0xc0ddf
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc0de1
    xor ah, ah                                ; 30 e4                       ; 0xc0de4
    mul di                                    ; f7 e7                       ; 0xc0de6
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0de8
    xor dh, dh                                ; 30 f6                       ; 0xc0deb
    mov di, ax                                ; 89 c7                       ; 0xc0ded
    add di, dx                                ; 01 d7                       ; 0xc0def
    add di, di                                ; 01 ff                       ; 0xc0df1
    add di, cx                                ; 01 cf                       ; 0xc0df3
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0df5 vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0df9
    push SS                                   ; 16                          ; 0xc0dfc vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0dfd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0dfe
    jmp short 00d83h                          ; eb 80                       ; 0xc0e01 vgabios.c:536
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0e03 vgabios.c:537
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0e07
    je short 00e5ah                           ; 74 4e                       ; 0xc0e0a
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0e0c
    jc short 00e81h                           ; 72 70                       ; 0xc0e0f
    jbe short 00e1ah                          ; 76 07                       ; 0xc0e11
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e13
    jbe short 00e33h                          ; 76 1b                       ; 0xc0e16
    jmp short 00e81h                          ; eb 67                       ; 0xc0e18
    xor dh, dh                                ; 30 f6                       ; 0xc0e1a vgabios.c:540
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e1c
    xor ah, ah                                ; 30 e4                       ; 0xc0e1f
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc0e21
    call 00cdah                               ; e8 b3 fe                    ; 0xc0e24
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc0e27 vgabios.c:541
    xor dh, dh                                ; 30 f6                       ; 0xc0e2a
    call 00cf4h                               ; e8 c5 fe                    ; 0xc0e2c
    xor ah, ah                                ; 30 e4                       ; 0xc0e2f
    jmp short 00dfch                          ; eb c9                       ; 0xc0e31
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e33 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e36
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0e39 vgabios.c:546
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0e3c
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0e3f
    xor dh, dh                                ; 30 f6                       ; 0xc0e42
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e44
    xor ah, ah                                ; 30 e4                       ; 0xc0e47
    mov bx, di                                ; 89 fb                       ; 0xc0e49
    call 00afeh                               ; e8 b0 fc                    ; 0xc0e4b
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0e4e vgabios.c:547
    mov dx, ax                                ; 89 c2                       ; 0xc0e51
    mov ax, di                                ; 89 f8                       ; 0xc0e53
    call 00b2dh                               ; e8 d5 fc                    ; 0xc0e55
    jmp short 00e2fh                          ; eb d5                       ; 0xc0e58
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e5a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e5d
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0e60 vgabios.c:551
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0e63
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0e66
    xor dh, dh                                ; 30 f6                       ; 0xc0e69
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e6b
    xor ah, ah                                ; 30 e4                       ; 0xc0e6e
    mov bx, di                                ; 89 fb                       ; 0xc0e70
    call 00b6dh                               ; e8 f8 fc                    ; 0xc0e72
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0e75 vgabios.c:552
    mov dx, ax                                ; 89 c2                       ; 0xc0e78
    mov ax, di                                ; 89 f8                       ; 0xc0e7a
    call 00bdch                               ; e8 5d fd                    ; 0xc0e7c
    jmp short 00e2fh                          ; eb ae                       ; 0xc0e7f
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e81 vgabios.c:561
    pop di                                    ; 5f                          ; 0xc0e84
    pop si                                    ; 5e                          ; 0xc0e85
    pop cx                                    ; 59                          ; 0xc0e86
    pop bx                                    ; 5b                          ; 0xc0e87
    pop bp                                    ; 5d                          ; 0xc0e88
    retn                                      ; c3                          ; 0xc0e89
  ; disGetNextSymbol 0xc0e8a LB 0x33e5 -> off=0x10 cb=0000000000000083 uValue=00000000000c0e9a 'vga_get_font_info'
    db  0b1h, 00eh, 0f6h, 00eh, 0fbh, 00eh, 002h, 00fh, 007h, 00fh, 00ch, 00fh, 011h, 00fh, 016h, 00fh
vga_get_font_info:                           ; 0xc0e9a LB 0x83
    push si                                   ; 56                          ; 0xc0e9a vgabios.c:563
    push di                                   ; 57                          ; 0xc0e9b
    push bp                                   ; 55                          ; 0xc0e9c
    mov bp, sp                                ; 89 e5                       ; 0xc0e9d
    mov si, dx                                ; 89 d6                       ; 0xc0e9f
    mov di, bx                                ; 89 df                       ; 0xc0ea1
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0ea3 vgabios.c:568
    jnbe short 00ef0h                         ; 77 48                       ; 0xc0ea6
    mov bx, ax                                ; 89 c3                       ; 0xc0ea8
    add bx, ax                                ; 01 c3                       ; 0xc0eaa
    jmp word [cs:bx+00e8ah]                   ; 2e ff a7 8a 0e              ; 0xc0eac
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0eb1 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0eb4
    mov es, ax                                ; 8e c0                       ; 0xc0eb6
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0eb8
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0ebb
    push SS                                   ; 16                          ; 0xc0ebf vgabios.c:571
    pop ES                                    ; 07                          ; 0xc0ec0
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0ec1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0ec4
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0ec7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eca
    mov es, ax                                ; 8e c0                       ; 0xc0ecd
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ecf
    xor ah, ah                                ; 30 e4                       ; 0xc0ed2
    push SS                                   ; 16                          ; 0xc0ed4
    pop ES                                    ; 07                          ; 0xc0ed5
    mov bx, cx                                ; 89 cb                       ; 0xc0ed6
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ed8
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0edb
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ede
    mov es, ax                                ; 8e c0                       ; 0xc0ee1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ee3
    xor ah, ah                                ; 30 e4                       ; 0xc0ee6
    push SS                                   ; 16                          ; 0xc0ee8
    pop ES                                    ; 07                          ; 0xc0ee9
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0eea
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0eed
    pop bp                                    ; 5d                          ; 0xc0ef0
    pop di                                    ; 5f                          ; 0xc0ef1
    pop si                                    ; 5e                          ; 0xc0ef2
    retn 00002h                               ; c2 02 00                    ; 0xc0ef3
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0ef6 vgabios.c:57
    jmp short 00eb4h                          ; eb b9                       ; 0xc0ef9
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc0efb vgabios.c:576
    mov ax, ds                                ; 8c d8                       ; 0xc0efe
    jmp short 00ebfh                          ; eb bd                       ; 0xc0f00 vgabios.c:577
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc0f02 vgabios.c:579
    jmp short 00efeh                          ; eb f7                       ; 0xc0f05
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc0f07 vgabios.c:582
    jmp short 00efeh                          ; eb f2                       ; 0xc0f0a
    mov dx, 07b6ch                            ; ba 6c 7b                    ; 0xc0f0c vgabios.c:585
    jmp short 00efeh                          ; eb ed                       ; 0xc0f0f
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc0f11 vgabios.c:588
    jmp short 00efeh                          ; eb e8                       ; 0xc0f14
    mov dx, 07c99h                            ; ba 99 7c                    ; 0xc0f16 vgabios.c:591
    jmp short 00efeh                          ; eb e3                       ; 0xc0f19
    jmp short 00ef0h                          ; eb d3                       ; 0xc0f1b vgabios.c:597
  ; disGetNextSymbol 0xc0f1d LB 0x3352 -> off=0x0 cb=0000000000000166 uValue=00000000000c0f1d 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f1d LB 0x166
    push bp                                   ; 55                          ; 0xc0f1d vgabios.c:610
    mov bp, sp                                ; 89 e5                       ; 0xc0f1e
    push si                                   ; 56                          ; 0xc0f20
    push di                                   ; 57                          ; 0xc0f21
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0f22
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0f25
    mov si, dx                                ; 89 d6                       ; 0xc0f28
    mov dx, bx                                ; 89 da                       ; 0xc0f2a
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc0f2c
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0f2f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f32
    mov es, ax                                ; 8e c0                       ; 0xc0f35
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f37
    xor ah, ah                                ; 30 e4                       ; 0xc0f3a vgabios.c:617
    call 035b3h                               ; e8 74 26                    ; 0xc0f3c
    mov ah, al                                ; 88 c4                       ; 0xc0f3f
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f41 vgabios.c:618
    je short 00f53h                           ; 74 0e                       ; 0xc0f43
    mov bl, al                                ; 88 c3                       ; 0xc0f45 vgabios.c:620
    xor bh, bh                                ; 30 ff                       ; 0xc0f47
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0f49
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f4c
    jne short 00f56h                          ; 75 03                       ; 0xc0f51
    jmp near 0107ch                           ; e9 26 01                    ; 0xc0f53 vgabios.c:621
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f56 vgabios.c:624
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0f5a
    jc short 00f6eh                           ; 72 0f                       ; 0xc0f5d
    jbe short 00f76h                          ; 76 15                       ; 0xc0f5f
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f61
    je short 00fadh                           ; 74 47                       ; 0xc0f64
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f66
    je short 00f76h                           ; 74 0b                       ; 0xc0f69
    jmp near 01072h                           ; e9 04 01                    ; 0xc0f6b
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f6e
    je short 00fe4h                           ; 74 71                       ; 0xc0f71
    jmp near 01072h                           ; e9 fc 00                    ; 0xc0f73
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0f76 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f79
    mov es, ax                                ; 8e c0                       ; 0xc0f7c
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc0f7e
    mov ax, dx                                ; 89 d0                       ; 0xc0f81 vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc0f83
    mov bx, si                                ; 89 f3                       ; 0xc0f85
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0f87
    add bx, ax                                ; 01 c3                       ; 0xc0f8a
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0f8c vgabios.c:47
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0f8f
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0f92 vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc0f95
    mul dx                                    ; f7 e2                       ; 0xc0f97
    add bx, ax                                ; 01 c3                       ; 0xc0f99
    mov cx, si                                ; 89 f1                       ; 0xc0f9b vgabios.c:629
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0f9d
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0fa0
    sar ax, CL                                ; d3 f8                       ; 0xc0fa3
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0fa5
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0fa8 vgabios.c:631
    jmp short 00fb6h                          ; eb 09                       ; 0xc0fab
    jmp near 01052h                           ; e9 a2 00                    ; 0xc0fad
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0fb0
    jnc short 00fe1h                          ; 73 2b                       ; 0xc0fb4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0fb6 vgabios.c:632
    xor ah, ah                                ; 30 e4                       ; 0xc0fb9
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0fbb
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0fbe
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0fc0
    out DX, ax                                ; ef                          ; 0xc0fc3
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0fc4 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0fc7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fc9
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0fcc vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0fcf vgabios.c:634
    jbe short 00fdch                          ; 76 09                       ; 0xc0fd1
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0fd3 vgabios.c:635
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0fd6
    sal al, CL                                ; d2 e0                       ; 0xc0fd8
    or ch, al                                 ; 08 c5                       ; 0xc0fda
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0fdc vgabios.c:636
    jmp short 00fb0h                          ; eb cf                       ; 0xc0fdf
    jmp near 01074h                           ; e9 90 00                    ; 0xc0fe1
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc0fe4 vgabios.c:639
    xor ch, ch                                ; 30 ed                       ; 0xc0fe8
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc0fea
    sub bx, cx                                ; 29 cb                       ; 0xc0fed
    mov cx, bx                                ; 89 d9                       ; 0xc0fef
    mov bx, si                                ; 89 f3                       ; 0xc0ff1
    shr bx, CL                                ; d3 eb                       ; 0xc0ff3
    mov cx, bx                                ; 89 d9                       ; 0xc0ff5
    mov bx, dx                                ; 89 d3                       ; 0xc0ff7
    shr bx, 1                                 ; d1 eb                       ; 0xc0ff9
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc0ffb
    add bx, cx                                ; 01 cb                       ; 0xc0ffe
    test dl, 001h                             ; f6 c2 01                    ; 0xc1000 vgabios.c:640
    je short 01008h                           ; 74 03                       ; 0xc1003
    add bh, 020h                              ; 80 c7 20                    ; 0xc1005 vgabios.c:641
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1008 vgabios.c:37
    mov es, dx                                ; 8e c2                       ; 0xc100b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc100d
    mov bl, ah                                ; 88 e3                       ; 0xc1010 vgabios.c:643
    xor bh, bh                                ; 30 ff                       ; 0xc1012
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1014
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc1017
    jne short 01039h                          ; 75 1b                       ; 0xc101c
    mov cx, si                                ; 89 f1                       ; 0xc101e vgabios.c:644
    xor ch, ch                                ; 30 ed                       ; 0xc1020
    and cl, 003h                              ; 80 e1 03                    ; 0xc1022
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc1025
    sub dx, cx                                ; 29 ca                       ; 0xc1028
    mov cx, dx                                ; 89 d1                       ; 0xc102a
    add cx, dx                                ; 01 d1                       ; 0xc102c
    xor ah, ah                                ; 30 e4                       ; 0xc102e
    sar ax, CL                                ; d3 f8                       ; 0xc1030
    mov ch, al                                ; 88 c5                       ; 0xc1032
    and ch, 003h                              ; 80 e5 03                    ; 0xc1034
    jmp short 01074h                          ; eb 3b                       ; 0xc1037 vgabios.c:645
    mov cx, si                                ; 89 f1                       ; 0xc1039 vgabios.c:646
    xor ch, ch                                ; 30 ed                       ; 0xc103b
    and cl, 007h                              ; 80 e1 07                    ; 0xc103d
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc1040
    sub dx, cx                                ; 29 ca                       ; 0xc1043
    mov cx, dx                                ; 89 d1                       ; 0xc1045
    xor ah, ah                                ; 30 e4                       ; 0xc1047
    sar ax, CL                                ; d3 f8                       ; 0xc1049
    mov ch, al                                ; 88 c5                       ; 0xc104b
    and ch, 001h                              ; 80 e5 01                    ; 0xc104d
    jmp short 01074h                          ; eb 22                       ; 0xc1050 vgabios.c:647
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1052 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1055
    mov es, ax                                ; 8e c0                       ; 0xc1058
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc105a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc105d vgabios.c:48
    mov ax, dx                                ; 89 d0                       ; 0xc1060
    mul bx                                    ; f7 e3                       ; 0xc1062
    mov bx, si                                ; 89 f3                       ; 0xc1064
    add bx, ax                                ; 01 c3                       ; 0xc1066
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1068 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc106b
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc106d
    jmp short 01074h                          ; eb 02                       ; 0xc1070 vgabios.c:651
    xor ch, ch                                ; 30 ed                       ; 0xc1072 vgabios.c:656
    push SS                                   ; 16                          ; 0xc1074 vgabios.c:658
    pop ES                                    ; 07                          ; 0xc1075
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1076
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc1079
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc107c vgabios.c:659
    pop di                                    ; 5f                          ; 0xc107f
    pop si                                    ; 5e                          ; 0xc1080
    pop bp                                    ; 5d                          ; 0xc1081
    retn                                      ; c3                          ; 0xc1082
  ; disGetNextSymbol 0xc1083 LB 0x31ec -> off=0x0 cb=000000000000008d uValue=00000000000c1083 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc1083 LB 0x8d
    push bp                                   ; 55                          ; 0xc1083 vgabios.c:664
    mov bp, sp                                ; 89 e5                       ; 0xc1084
    push bx                                   ; 53                          ; 0xc1086
    push cx                                   ; 51                          ; 0xc1087
    push si                                   ; 56                          ; 0xc1088
    push di                                   ; 57                          ; 0xc1089
    push ax                                   ; 50                          ; 0xc108a
    push ax                                   ; 50                          ; 0xc108b
    mov bx, ax                                ; 89 c3                       ; 0xc108c
    mov di, dx                                ; 89 d7                       ; 0xc108e
    mov dx, 003dah                            ; ba da 03                    ; 0xc1090 vgabios.c:669
    in AL, DX                                 ; ec                          ; 0xc1093
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1094
    xor al, al                                ; 30 c0                       ; 0xc1096 vgabios.c:670
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1098
    out DX, AL                                ; ee                          ; 0xc109b
    xor si, si                                ; 31 f6                       ; 0xc109c vgabios.c:672
    cmp si, di                                ; 39 fe                       ; 0xc109e
    jnc short 010f5h                          ; 73 53                       ; 0xc10a0
    mov al, bl                                ; 88 d8                       ; 0xc10a2 vgabios.c:675
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc10a4
    out DX, AL                                ; ee                          ; 0xc10a7
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10a8 vgabios.c:677
    in AL, DX                                 ; ec                          ; 0xc10ab
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10ac
    mov cx, ax                                ; 89 c1                       ; 0xc10ae
    in AL, DX                                 ; ec                          ; 0xc10b0 vgabios.c:678
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10b1
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc10b3
    in AL, DX                                 ; ec                          ; 0xc10b6 vgabios.c:679
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10b7
    xor ch, ch                                ; 30 ed                       ; 0xc10b9 vgabios.c:682
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc10bb
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc10be
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc10c1
    xor ch, ch                                ; 30 ed                       ; 0xc10c4
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc10c6
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc10ca
    xor ah, ah                                ; 30 e4                       ; 0xc10cd
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc10cf
    add cx, ax                                ; 01 c1                       ; 0xc10d2
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc10d4
    sar cx, 008h                              ; c1 f9 08                    ; 0xc10d8
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc10db vgabios.c:684
    jbe short 010e3h                          ; 76 03                       ; 0xc10de
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc10e0
    mov al, bl                                ; 88 d8                       ; 0xc10e3 vgabios.c:687
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc10e5
    out DX, AL                                ; ee                          ; 0xc10e8
    mov al, cl                                ; 88 c8                       ; 0xc10e9 vgabios.c:689
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10eb
    out DX, AL                                ; ee                          ; 0xc10ee
    out DX, AL                                ; ee                          ; 0xc10ef vgabios.c:690
    out DX, AL                                ; ee                          ; 0xc10f0 vgabios.c:691
    inc bx                                    ; 43                          ; 0xc10f1 vgabios.c:692
    inc si                                    ; 46                          ; 0xc10f2 vgabios.c:693
    jmp short 0109eh                          ; eb a9                       ; 0xc10f3
    mov dx, 003dah                            ; ba da 03                    ; 0xc10f5 vgabios.c:694
    in AL, DX                                 ; ec                          ; 0xc10f8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10f9
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc10fb vgabios.c:695
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10fd
    out DX, AL                                ; ee                          ; 0xc1100
    mov dx, 003dah                            ; ba da 03                    ; 0xc1101 vgabios.c:697
    in AL, DX                                 ; ec                          ; 0xc1104
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1105
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1107 vgabios.c:699
    pop di                                    ; 5f                          ; 0xc110a
    pop si                                    ; 5e                          ; 0xc110b
    pop cx                                    ; 59                          ; 0xc110c
    pop bx                                    ; 5b                          ; 0xc110d
    pop bp                                    ; 5d                          ; 0xc110e
    retn                                      ; c3                          ; 0xc110f
  ; disGetNextSymbol 0xc1110 LB 0x315f -> off=0x0 cb=0000000000000107 uValue=00000000000c1110 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc1110 LB 0x107
    push bp                                   ; 55                          ; 0xc1110 vgabios.c:702
    mov bp, sp                                ; 89 e5                       ; 0xc1111
    push bx                                   ; 53                          ; 0xc1113
    push cx                                   ; 51                          ; 0xc1114
    push si                                   ; 56                          ; 0xc1115
    push ax                                   ; 50                          ; 0xc1116
    push ax                                   ; 50                          ; 0xc1117
    mov bl, al                                ; 88 c3                       ; 0xc1118
    mov ah, dl                                ; 88 d4                       ; 0xc111a
    mov dl, al                                ; 88 c2                       ; 0xc111c vgabios.c:708
    xor dh, dh                                ; 30 f6                       ; 0xc111e
    mov cx, dx                                ; 89 d1                       ; 0xc1120
    sal cx, 008h                              ; c1 e1 08                    ; 0xc1122
    mov dl, ah                                ; 88 e2                       ; 0xc1125
    add dx, cx                                ; 01 ca                       ; 0xc1127
    mov si, strict word 00060h                ; be 60 00                    ; 0xc1129 vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc112c
    mov es, cx                                ; 8e c1                       ; 0xc112f
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc1131
    mov si, 00087h                            ; be 87 00                    ; 0xc1134 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1137
    test dl, 008h                             ; f6 c2 08                    ; 0xc113a vgabios.c:38
    jne short 0117ch                          ; 75 3d                       ; 0xc113d
    mov dl, al                                ; 88 c2                       ; 0xc113f vgabios.c:714
    and dl, 060h                              ; 80 e2 60                    ; 0xc1141
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc1144
    jne short 0114fh                          ; 75 06                       ; 0xc1147
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc1149 vgabios.c:716
    xor ah, ah                                ; 30 e4                       ; 0xc114b vgabios.c:717
    jmp short 0117ch                          ; eb 2d                       ; 0xc114d vgabios.c:718
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc114f vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc1152 vgabios.c:38
    jne short 011b1h                          ; 75 5a                       ; 0xc1155
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc1157
    jnc short 011b1h                          ; 73 55                       ; 0xc115a
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc115c
    jnc short 011b1h                          ; 73 50                       ; 0xc115f
    mov si, 00085h                            ; be 85 00                    ; 0xc1161 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1164
    mov es, dx                                ; 8e c2                       ; 0xc1167
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1169
    mov dx, cx                                ; 89 ca                       ; 0xc116c vgabios.c:48
    cmp ah, bl                                ; 38 dc                       ; 0xc116e vgabios.c:729
    jnc short 0117eh                          ; 73 0c                       ; 0xc1170
    test ah, ah                               ; 84 e4                       ; 0xc1172 vgabios.c:731
    je short 011b1h                           ; 74 3b                       ; 0xc1174
    xor bl, bl                                ; 30 db                       ; 0xc1176 vgabios.c:732
    mov ah, cl                                ; 88 cc                       ; 0xc1178 vgabios.c:733
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc117a
    jmp short 011b1h                          ; eb 33                       ; 0xc117c vgabios.c:735
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc117e vgabios.c:736
    xor al, al                                ; 30 c0                       ; 0xc1181
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc1183
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1186
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1189
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc118c
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc118f
    cmp si, cx                                ; 39 ce                       ; 0xc1192
    jnc short 011b3h                          ; 73 1d                       ; 0xc1194
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc1196
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1199
    mov si, cx                                ; 89 ce                       ; 0xc119c
    dec si                                    ; 4e                          ; 0xc119e
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc119f
    je short 011edh                           ; 74 49                       ; 0xc11a2
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11a4
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11a7
    dec cx                                    ; 49                          ; 0xc11aa
    dec cx                                    ; 49                          ; 0xc11ab
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc11ac
    jne short 011b3h                          ; 75 02                       ; 0xc11af
    jmp short 011edh                          ; eb 3a                       ; 0xc11b1
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc11b3 vgabios.c:738
    jbe short 011edh                          ; 76 35                       ; 0xc11b6
    mov cl, bl                                ; 88 d9                       ; 0xc11b8 vgabios.c:739
    xor ch, ch                                ; 30 ed                       ; 0xc11ba
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc11bc
    mov byte [bp-009h], ch                    ; 88 6e f7                    ; 0xc11bf
    mov si, cx                                ; 89 ce                       ; 0xc11c2
    inc si                                    ; 46                          ; 0xc11c4
    inc si                                    ; 46                          ; 0xc11c5
    mov cl, dl                                ; 88 d1                       ; 0xc11c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc11c8
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc11ca
    jl short 011e2h                           ; 7c 13                       ; 0xc11cd
    sub bl, ah                                ; 28 e3                       ; 0xc11cf vgabios.c:741
    add bl, dl                                ; 00 d3                       ; 0xc11d1
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11d3
    mov ah, cl                                ; 88 cc                       ; 0xc11d5 vgabios.c:742
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc11d7 vgabios.c:743
    jc short 011edh                           ; 72 11                       ; 0xc11da
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc11dc vgabios.c:745
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11de vgabios.c:746
    jmp short 011edh                          ; eb 0b                       ; 0xc11e0 vgabios.c:748
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc11e2
    jbe short 011ebh                          ; 76 04                       ; 0xc11e5
    shr dx, 1                                 ; d1 ea                       ; 0xc11e7 vgabios.c:750
    mov bl, dl                                ; 88 d3                       ; 0xc11e9
    mov ah, cl                                ; 88 cc                       ; 0xc11eb vgabios.c:754
    mov si, strict word 00063h                ; be 63 00                    ; 0xc11ed vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc11f0
    mov es, dx                                ; 8e c2                       ; 0xc11f3
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11f5
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc11f8 vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc11fa
    out DX, AL                                ; ee                          ; 0xc11fc
    mov si, cx                                ; 89 ce                       ; 0xc11fd vgabios.c:766
    inc si                                    ; 46                          ; 0xc11ff
    mov al, bl                                ; 88 d8                       ; 0xc1200
    mov dx, si                                ; 89 f2                       ; 0xc1202
    out DX, AL                                ; ee                          ; 0xc1204
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc1205 vgabios.c:767
    mov dx, cx                                ; 89 ca                       ; 0xc1207
    out DX, AL                                ; ee                          ; 0xc1209
    mov al, ah                                ; 88 e0                       ; 0xc120a vgabios.c:768
    mov dx, si                                ; 89 f2                       ; 0xc120c
    out DX, AL                                ; ee                          ; 0xc120e
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc120f vgabios.c:769
    pop si                                    ; 5e                          ; 0xc1212
    pop cx                                    ; 59                          ; 0xc1213
    pop bx                                    ; 5b                          ; 0xc1214
    pop bp                                    ; 5d                          ; 0xc1215
    retn                                      ; c3                          ; 0xc1216
  ; disGetNextSymbol 0xc1217 LB 0x3058 -> off=0x0 cb=000000000000008f uValue=00000000000c1217 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc1217 LB 0x8f
    push bp                                   ; 55                          ; 0xc1217 vgabios.c:772
    mov bp, sp                                ; 89 e5                       ; 0xc1218
    push bx                                   ; 53                          ; 0xc121a
    push cx                                   ; 51                          ; 0xc121b
    push si                                   ; 56                          ; 0xc121c
    push di                                   ; 57                          ; 0xc121d
    push ax                                   ; 50                          ; 0xc121e
    mov bl, al                                ; 88 c3                       ; 0xc121f
    mov cx, dx                                ; 89 d1                       ; 0xc1221
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1223 vgabios.c:778
    jnbe short 0129dh                         ; 77 76                       ; 0xc1225
    xor ah, ah                                ; 30 e4                       ; 0xc1227 vgabios.c:781
    mov si, ax                                ; 89 c6                       ; 0xc1229
    add si, ax                                ; 01 c6                       ; 0xc122b
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc122d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1230 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc1233
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc1235
    mov si, strict word 00062h                ; be 62 00                    ; 0xc1238 vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc123b
    cmp bl, al                                ; 38 c3                       ; 0xc123e vgabios.c:785
    jne short 0129dh                          ; 75 5b                       ; 0xc1240
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc1242 vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc1245
    mov si, 00084h                            ; be 84 00                    ; 0xc1248 vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc124b
    xor ah, ah                                ; 30 e4                       ; 0xc124e vgabios.c:38
    mov si, ax                                ; 89 c6                       ; 0xc1250
    inc si                                    ; 46                          ; 0xc1252
    mov ax, dx                                ; 89 d0                       ; 0xc1253 vgabios.c:791
    xor al, dl                                ; 30 d0                       ; 0xc1255
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1257
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc125a
    mov ax, di                                ; 89 f8                       ; 0xc125d vgabios.c:794
    mul si                                    ; f7 e6                       ; 0xc125f
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1261
    xor bh, bh                                ; 30 ff                       ; 0xc1263
    inc ax                                    ; 40                          ; 0xc1265
    mul bx                                    ; f7 e3                       ; 0xc1266
    mov bl, cl                                ; 88 cb                       ; 0xc1268
    mov si, bx                                ; 89 de                       ; 0xc126a
    add si, ax                                ; 01 c6                       ; 0xc126c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc126e
    xor ah, ah                                ; 30 e4                       ; 0xc1271
    mul di                                    ; f7 e7                       ; 0xc1273
    add si, ax                                ; 01 c6                       ; 0xc1275
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1277 vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc127a
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc127d vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc127f
    out DX, AL                                ; ee                          ; 0xc1281
    mov ax, si                                ; 89 f0                       ; 0xc1282 vgabios.c:799
    xor al, al                                ; 30 c0                       ; 0xc1284
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1286
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc1289
    mov dx, cx                                ; 89 ca                       ; 0xc128c
    out DX, AL                                ; ee                          ; 0xc128e
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc128f vgabios.c:800
    mov dx, bx                                ; 89 da                       ; 0xc1291
    out DX, AL                                ; ee                          ; 0xc1293
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc1294 vgabios.c:801
    mov ax, si                                ; 89 f0                       ; 0xc1298
    mov dx, cx                                ; 89 ca                       ; 0xc129a
    out DX, AL                                ; ee                          ; 0xc129c
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc129d vgabios.c:803
    pop di                                    ; 5f                          ; 0xc12a0
    pop si                                    ; 5e                          ; 0xc12a1
    pop cx                                    ; 59                          ; 0xc12a2
    pop bx                                    ; 5b                          ; 0xc12a3
    pop bp                                    ; 5d                          ; 0xc12a4
    retn                                      ; c3                          ; 0xc12a5
  ; disGetNextSymbol 0xc12a6 LB 0x2fc9 -> off=0x0 cb=00000000000000d8 uValue=00000000000c12a6 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc12a6 LB 0xd8
    push bp                                   ; 55                          ; 0xc12a6 vgabios.c:806
    mov bp, sp                                ; 89 e5                       ; 0xc12a7
    push bx                                   ; 53                          ; 0xc12a9
    push cx                                   ; 51                          ; 0xc12aa
    push dx                                   ; 52                          ; 0xc12ab
    push si                                   ; 56                          ; 0xc12ac
    push di                                   ; 57                          ; 0xc12ad
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc12ae
    mov cl, al                                ; 88 c1                       ; 0xc12b1
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12b3 vgabios.c:812
    jnbe short 012cdh                         ; 77 16                       ; 0xc12b5
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc12b7 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ba
    mov es, ax                                ; 8e c0                       ; 0xc12bd
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12bf
    xor ah, ah                                ; 30 e4                       ; 0xc12c2 vgabios.c:816
    call 035b3h                               ; e8 ec 22                    ; 0xc12c4
    mov ch, al                                ; 88 c5                       ; 0xc12c7
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc12c9 vgabios.c:817
    jne short 012d0h                          ; 75 03                       ; 0xc12cb
    jmp near 01374h                           ; e9 a4 00                    ; 0xc12cd
    mov al, cl                                ; 88 c8                       ; 0xc12d0 vgabios.c:820
    xor ah, ah                                ; 30 e4                       ; 0xc12d2
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc12d4
    lea dx, [bp-010h]                         ; 8d 56 f0                    ; 0xc12d7
    call 00a1ah                               ; e8 3d f7                    ; 0xc12da
    mov bl, ch                                ; 88 eb                       ; 0xc12dd vgabios.c:822
    xor bh, bh                                ; 30 ff                       ; 0xc12df
    mov si, bx                                ; 89 de                       ; 0xc12e1
    sal si, 003h                              ; c1 e6 03                    ; 0xc12e3
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc12e6
    jne short 0132ch                          ; 75 3f                       ; 0xc12eb
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc12ed vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12f0
    mov es, ax                                ; 8e c0                       ; 0xc12f3
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc12f5
    mov bx, 00084h                            ; bb 84 00                    ; 0xc12f8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12fb
    xor ah, ah                                ; 30 e4                       ; 0xc12fe vgabios.c:38
    mov bx, ax                                ; 89 c3                       ; 0xc1300
    inc bx                                    ; 43                          ; 0xc1302
    mov ax, dx                                ; 89 d0                       ; 0xc1303 vgabios.c:829
    mul bx                                    ; f7 e3                       ; 0xc1305
    mov di, ax                                ; 89 c7                       ; 0xc1307
    add ax, ax                                ; 01 c0                       ; 0xc1309
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc130b
    mov byte [bp-00ch], cl                    ; 88 4e f4                    ; 0xc130d
    mov byte [bp-00bh], 000h                  ; c6 46 f5 00                 ; 0xc1310
    inc ax                                    ; 40                          ; 0xc1314
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc1315
    mov bx, ax                                ; 89 c3                       ; 0xc1318
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc131a vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc131d
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc1320 vgabios.c:833
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc1324
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc1327
    jmp short 0133bh                          ; eb 0f                       ; 0xc132a vgabios.c:835
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc132c vgabios.c:837
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1330
    mov al, cl                                ; 88 c8                       ; 0xc1333
    xor ah, ah                                ; 30 e4                       ; 0xc1335
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc1337
    mov bx, ax                                ; 89 c3                       ; 0xc133b
    mov si, strict word 00063h                ; be 63 00                    ; 0xc133d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1340
    mov es, ax                                ; 8e c0                       ; 0xc1343
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc1345
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc1348 vgabios.c:842
    mov dx, si                                ; 89 f2                       ; 0xc134a
    out DX, AL                                ; ee                          ; 0xc134c
    mov ax, bx                                ; 89 d8                       ; 0xc134d vgabios.c:843
    xor al, bl                                ; 30 d8                       ; 0xc134f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1351
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc1354
    mov dx, di                                ; 89 fa                       ; 0xc1357
    out DX, AL                                ; ee                          ; 0xc1359
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc135a vgabios.c:844
    mov dx, si                                ; 89 f2                       ; 0xc135c
    out DX, AL                                ; ee                          ; 0xc135e
    xor bh, bh                                ; 30 ff                       ; 0xc135f vgabios.c:845
    mov ax, bx                                ; 89 d8                       ; 0xc1361
    mov dx, di                                ; 89 fa                       ; 0xc1363
    out DX, AL                                ; ee                          ; 0xc1365
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1366 vgabios.c:42
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc1369
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc136c vgabios.c:855
    mov al, cl                                ; 88 c8                       ; 0xc136f
    call 01217h                               ; e8 a3 fe                    ; 0xc1371
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1374 vgabios.c:856
    pop di                                    ; 5f                          ; 0xc1377
    pop si                                    ; 5e                          ; 0xc1378
    pop dx                                    ; 5a                          ; 0xc1379
    pop cx                                    ; 59                          ; 0xc137a
    pop bx                                    ; 5b                          ; 0xc137b
    pop bp                                    ; 5d                          ; 0xc137c
    retn                                      ; c3                          ; 0xc137d
  ; disGetNextSymbol 0xc137e LB 0x2ef1 -> off=0x0 cb=0000000000000375 uValue=00000000000c137e 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc137e LB 0x375
    push bp                                   ; 55                          ; 0xc137e vgabios.c:876
    mov bp, sp                                ; 89 e5                       ; 0xc137f
    push bx                                   ; 53                          ; 0xc1381
    push cx                                   ; 51                          ; 0xc1382
    push dx                                   ; 52                          ; 0xc1383
    push si                                   ; 56                          ; 0xc1384
    push di                                   ; 57                          ; 0xc1385
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1386
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1389
    and AL, strict byte 080h                  ; 24 80                       ; 0xc138c vgabios.c:880
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc138e
    call 007afh                               ; e8 1b f4                    ; 0xc1391 vgabios.c:888
    test ax, ax                               ; 85 c0                       ; 0xc1394
    je short 013a4h                           ; 74 0c                       ; 0xc1396
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1398 vgabios.c:890
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc139a
    out DX, AL                                ; ee                          ; 0xc139d
    xor al, al                                ; 30 c0                       ; 0xc139e vgabios.c:891
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc13a0
    out DX, AL                                ; ee                          ; 0xc13a3
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc13a4 vgabios.c:896
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc13a8 vgabios.c:900
    jne short 013b2h                          ; 75 04                       ; 0xc13ac
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc13ae vgabios.c:901
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc13b2 vgabios.c:904
    xor ah, ah                                ; 30 e4                       ; 0xc13b5
    call 035b3h                               ; e8 f9 21                    ; 0xc13b7
    mov bl, al                                ; 88 c3                       ; 0xc13ba
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc13bc
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc13bf vgabios.c:910
    je short 01420h                           ; 74 5d                       ; 0xc13c1
    xor bh, bh                                ; 30 ff                       ; 0xc13c3 vgabios.c:913
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc13c5
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc13c9
    mov di, 00089h                            ; bf 89 00                    ; 0xc13cc vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13cf
    mov es, ax                                ; 8e c0                       ; 0xc13d2
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc13d4
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc13d7 vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc13da vgabios.c:930
    jne short 01423h                          ; 75 45                       ; 0xc13dc
    sal bx, 003h                              ; c1 e3 03                    ; 0xc13de vgabios.c:932
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc13e1
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc13e5
    out DX, AL                                ; ee                          ; 0xc13e8
    xor al, al                                ; 30 c0                       ; 0xc13e9 vgabios.c:935
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc13eb
    out DX, AL                                ; ee                          ; 0xc13ee
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc13ef vgabios.c:938
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc13f3
    jc short 01406h                           ; 72 0e                       ; 0xc13f6
    jbe short 0140fh                          ; 76 15                       ; 0xc13f8
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc13fa
    je short 01419h                           ; 74 1a                       ; 0xc13fd
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc13ff
    je short 01414h                           ; 74 10                       ; 0xc1402
    jmp short 0141ch                          ; eb 16                       ; 0xc1404
    test bl, bl                               ; 84 db                       ; 0xc1406
    jne short 0141ch                          ; 75 12                       ; 0xc1408
    mov si, 04fc2h                            ; be c2 4f                    ; 0xc140a vgabios.c:940
    jmp short 0141ch                          ; eb 0d                       ; 0xc140d vgabios.c:941
    mov si, 05082h                            ; be 82 50                    ; 0xc140f vgabios.c:943
    jmp short 0141ch                          ; eb 08                       ; 0xc1412 vgabios.c:944
    mov si, 05142h                            ; be 42 51                    ; 0xc1414 vgabios.c:946
    jmp short 0141ch                          ; eb 03                       ; 0xc1417 vgabios.c:947
    mov si, 05202h                            ; be 02 52                    ; 0xc1419 vgabios.c:949
    xor cx, cx                                ; 31 c9                       ; 0xc141c vgabios.c:953
    jmp short 0142bh                          ; eb 0b                       ; 0xc141e
    jmp near 016e9h                           ; e9 c6 02                    ; 0xc1420
    jmp short 01471h                          ; eb 4c                       ; 0xc1423
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc1425
    jnc short 01463h                          ; 73 38                       ; 0xc1429
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc142b vgabios.c:954
    xor bh, bh                                ; 30 ff                       ; 0xc142e
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1430
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc1433
    xor bh, bh                                ; 30 ff                       ; 0xc1437
    mov al, byte [bx+0483eh]                  ; 8a 87 3e 48                 ; 0xc1439
    xor ah, ah                                ; 30 e4                       ; 0xc143d
    cmp cx, ax                                ; 39 c1                       ; 0xc143f
    jnbe short 01458h                         ; 77 15                       ; 0xc1441
    imul bx, cx, strict byte 00003h           ; 6b d9 03                    ; 0xc1443 vgabios.c:955
    add bx, si                                ; 01 f3                       ; 0xc1446
    mov al, byte [bx]                         ; 8a 07                       ; 0xc1448
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc144a
    out DX, AL                                ; ee                          ; 0xc144d
    mov al, byte [bx+001h]                    ; 8a 47 01                    ; 0xc144e vgabios.c:956
    out DX, AL                                ; ee                          ; 0xc1451
    mov al, byte [bx+002h]                    ; 8a 47 02                    ; 0xc1452 vgabios.c:957
    out DX, AL                                ; ee                          ; 0xc1455
    jmp short 01460h                          ; eb 08                       ; 0xc1456 vgabios.c:959
    xor al, al                                ; 30 c0                       ; 0xc1458 vgabios.c:960
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc145a
    out DX, AL                                ; ee                          ; 0xc145d
    out DX, AL                                ; ee                          ; 0xc145e vgabios.c:961
    out DX, AL                                ; ee                          ; 0xc145f vgabios.c:962
    inc cx                                    ; 41                          ; 0xc1460 vgabios.c:964
    jmp short 01425h                          ; eb c2                       ; 0xc1461
    test byte [bp-014h], 002h                 ; f6 46 ec 02                 ; 0xc1463 vgabios.c:965
    je short 01471h                           ; 74 08                       ; 0xc1467
    mov dx, 00100h                            ; ba 00 01                    ; 0xc1469 vgabios.c:967
    xor ax, ax                                ; 31 c0                       ; 0xc146c
    call 01083h                               ; e8 12 fc                    ; 0xc146e
    mov dx, 003dah                            ; ba da 03                    ; 0xc1471 vgabios.c:972
    in AL, DX                                 ; ec                          ; 0xc1474
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1475
    xor cx, cx                                ; 31 c9                       ; 0xc1477 vgabios.c:975
    jmp short 01480h                          ; eb 05                       ; 0xc1479
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc147b
    jnbe short 0149ah                         ; 77 1a                       ; 0xc147e
    mov al, cl                                ; 88 c8                       ; 0xc1480 vgabios.c:976
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1482
    out DX, AL                                ; ee                          ; 0xc1485
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1486 vgabios.c:977
    xor ah, ah                                ; 30 e4                       ; 0xc1489
    mov bx, ax                                ; 89 c3                       ; 0xc148b
    sal bx, 006h                              ; c1 e3 06                    ; 0xc148d
    add bx, cx                                ; 01 cb                       ; 0xc1490
    mov al, byte [bx+04865h]                  ; 8a 87 65 48                 ; 0xc1492
    out DX, AL                                ; ee                          ; 0xc1496
    inc cx                                    ; 41                          ; 0xc1497 vgabios.c:978
    jmp short 0147bh                          ; eb e1                       ; 0xc1498
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc149a vgabios.c:979
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc149c
    out DX, AL                                ; ee                          ; 0xc149f
    xor al, al                                ; 30 c0                       ; 0xc14a0 vgabios.c:980
    out DX, AL                                ; ee                          ; 0xc14a2
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14a3 vgabios.c:983
    out DX, AL                                ; ee                          ; 0xc14a6
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc14a7 vgabios.c:984
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc14a9
    out DX, AL                                ; ee                          ; 0xc14ac
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc14ad vgabios.c:985
    jmp short 014b7h                          ; eb 05                       ; 0xc14b0
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc14b2
    jnbe short 014d4h                         ; 77 1d                       ; 0xc14b5
    mov al, cl                                ; 88 c8                       ; 0xc14b7 vgabios.c:986
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14b9
    out DX, AL                                ; ee                          ; 0xc14bc
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc14bd vgabios.c:987
    xor ah, ah                                ; 30 e4                       ; 0xc14c0
    mov bx, ax                                ; 89 c3                       ; 0xc14c2
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14c4
    add bx, cx                                ; 01 cb                       ; 0xc14c7
    mov al, byte [bx+04846h]                  ; 8a 87 46 48                 ; 0xc14c9
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc14cd
    out DX, AL                                ; ee                          ; 0xc14d0
    inc cx                                    ; 41                          ; 0xc14d1 vgabios.c:988
    jmp short 014b2h                          ; eb de                       ; 0xc14d2
    xor cx, cx                                ; 31 c9                       ; 0xc14d4 vgabios.c:991
    jmp short 014ddh                          ; eb 05                       ; 0xc14d6
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc14d8
    jnbe short 014fah                         ; 77 1d                       ; 0xc14db
    mov al, cl                                ; 88 c8                       ; 0xc14dd vgabios.c:992
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc14df
    out DX, AL                                ; ee                          ; 0xc14e2
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc14e3 vgabios.c:993
    xor ah, ah                                ; 30 e4                       ; 0xc14e6
    mov bx, ax                                ; 89 c3                       ; 0xc14e8
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14ea
    add bx, cx                                ; 01 cb                       ; 0xc14ed
    mov al, byte [bx+04879h]                  ; 8a 87 79 48                 ; 0xc14ef
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc14f3
    out DX, AL                                ; ee                          ; 0xc14f6
    inc cx                                    ; 41                          ; 0xc14f7 vgabios.c:994
    jmp short 014d8h                          ; eb de                       ; 0xc14f8
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc14fa vgabios.c:997
    xor bh, bh                                ; 30 ff                       ; 0xc14fd
    sal bx, 003h                              ; c1 e3 03                    ; 0xc14ff
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc1502
    jne short 0150eh                          ; 75 05                       ; 0xc1507
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1509
    jmp short 01511h                          ; eb 03                       ; 0xc150c
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc150e
    mov si, dx                                ; 89 d6                       ; 0xc1511
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc1513 vgabios.c:1000
    out DX, ax                                ; ef                          ; 0xc1516
    xor cx, cx                                ; 31 c9                       ; 0xc1517 vgabios.c:1002
    jmp short 01520h                          ; eb 05                       ; 0xc1519
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc151b
    jnbe short 0153ch                         ; 77 1c                       ; 0xc151e
    mov al, cl                                ; 88 c8                       ; 0xc1520 vgabios.c:1003
    mov dx, si                                ; 89 f2                       ; 0xc1522
    out DX, AL                                ; ee                          ; 0xc1524
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc1525 vgabios.c:1004
    xor bh, bh                                ; 30 ff                       ; 0xc1528
    sal bx, 006h                              ; c1 e3 06                    ; 0xc152a
    mov di, bx                                ; 89 df                       ; 0xc152d
    add di, cx                                ; 01 cf                       ; 0xc152f
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc1531
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc1534
    out DX, AL                                ; ee                          ; 0xc1538
    inc cx                                    ; 41                          ; 0xc1539 vgabios.c:1005
    jmp short 0151bh                          ; eb df                       ; 0xc153a
    mov al, byte [bx+0484bh]                  ; 8a 87 4b 48                 ; 0xc153c vgabios.c:1008
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1540
    out DX, AL                                ; ee                          ; 0xc1543
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1544 vgabios.c:1011
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1546
    out DX, AL                                ; ee                          ; 0xc1549
    mov dx, 003dah                            ; ba da 03                    ; 0xc154a vgabios.c:1012
    in AL, DX                                 ; ec                          ; 0xc154d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc154e
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc1550 vgabios.c:1014
    jne short 015b3h                          ; 75 5d                       ; 0xc1554
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1556 vgabios.c:1016
    xor bh, bh                                ; 30 ff                       ; 0xc1559
    sal bx, 003h                              ; c1 e3 03                    ; 0xc155b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc155e
    jne short 01577h                          ; 75 12                       ; 0xc1563
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1565 vgabios.c:1018
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1569
    mov ax, 00720h                            ; b8 20 07                    ; 0xc156c
    xor di, di                                ; 31 ff                       ; 0xc156f
    jcxz 01575h                               ; e3 02                       ; 0xc1571
    rep stosw                                 ; f3 ab                       ; 0xc1573
    jmp short 015b3h                          ; eb 3c                       ; 0xc1575 vgabios.c:1020
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc1577 vgabios.c:1022
    jnc short 0158eh                          ; 73 11                       ; 0xc157b
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc157d vgabios.c:1024
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1581
    xor ax, ax                                ; 31 c0                       ; 0xc1584
    xor di, di                                ; 31 ff                       ; 0xc1586
    jcxz 0158ch                               ; e3 02                       ; 0xc1588
    rep stosw                                 ; f3 ab                       ; 0xc158a
    jmp short 015b3h                          ; eb 25                       ; 0xc158c vgabios.c:1026
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc158e vgabios.c:1028
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1590
    out DX, AL                                ; ee                          ; 0xc1593
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1594 vgabios.c:1029
    in AL, DX                                 ; ec                          ; 0xc1597
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1598
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc159a
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc159d vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc159f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15a0 vgabios.c:1031
    mov cx, 08000h                            ; b9 00 80                    ; 0xc15a4
    xor ax, ax                                ; 31 c0                       ; 0xc15a7
    xor di, di                                ; 31 ff                       ; 0xc15a9
    jcxz 015afh                               ; e3 02                       ; 0xc15ab
    rep stosw                                 ; f3 ab                       ; 0xc15ad
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc15af vgabios.c:1032
    out DX, AL                                ; ee                          ; 0xc15b2
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc15b3 vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15b6
    mov es, ax                                ; 8e c0                       ; 0xc15b9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc15bb
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15be
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc15c1 vgabios.c:1039
    xor bh, bh                                ; 30 ff                       ; 0xc15c4
    sal bx, 006h                              ; c1 e3 06                    ; 0xc15c6
    mov al, byte [bx+04842h]                  ; 8a 87 42 48                 ; 0xc15c9
    xor ah, ah                                ; 30 e4                       ; 0xc15cd
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc15cf vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc15d2
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc15d5 vgabios.c:50
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc15d9 vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc15dc
    mov di, strict word 00063h                ; bf 63 00                    ; 0xc15df vgabios.c:52
    mov word [es:di], si                      ; 26 89 35                    ; 0xc15e2
    mov al, byte [bx+04843h]                  ; 8a 87 43 48                 ; 0xc15e5 vgabios.c:40
    mov si, 00084h                            ; be 84 00                    ; 0xc15e9 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc15ec
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc15ef vgabios.c:1043
    xor ah, ah                                ; 30 e4                       ; 0xc15f3
    mov bx, 00085h                            ; bb 85 00                    ; 0xc15f5 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc15f8
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc15fb vgabios.c:1044
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc15fe
    mov bx, 00087h                            ; bb 87 00                    ; 0xc1600 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1603
    mov bx, 00088h                            ; bb 88 00                    ; 0xc1606 vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc1609
    mov bx, 00089h                            ; bb 89 00                    ; 0xc160d vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1610
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc1613 vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1615 vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc1618 vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc161b
    mov ax, ds                                ; 8c d8                       ; 0xc161f vgabios.c:1050
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc1621 vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc1624
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc1629
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc162d vgabios.c:1052
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1630
    jnbe short 0165bh                         ; 77 27                       ; 0xc1632
    mov bl, al                                ; 88 c3                       ; 0xc1634 vgabios.c:1054
    xor bh, bh                                ; 30 ff                       ; 0xc1636
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc1638 vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc163c vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc163f
    cmp byte [bp-00ch], 006h                  ; 80 7e f4 06                 ; 0xc1642 vgabios.c:1055
    jne short 0164dh                          ; 75 05                       ; 0xc1646
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc1648
    jmp short 01650h                          ; eb 03                       ; 0xc164b
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc164d
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc1650 vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1653
    mov es, dx                                ; 8e c2                       ; 0xc1656
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1658
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc165b vgabios.c:1059
    xor bh, bh                                ; 30 ff                       ; 0xc165e
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1660
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1663
    jne short 01673h                          ; 75 09                       ; 0xc1668
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc166a vgabios.c:1061
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc166d
    call 01110h                               ; e8 9d fa                    ; 0xc1670
    xor cx, cx                                ; 31 c9                       ; 0xc1673 vgabios.c:1065
    jmp short 0167ch                          ; eb 05                       ; 0xc1675
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc1677
    jnc short 01688h                          ; 73 0c                       ; 0xc167a
    mov al, cl                                ; 88 c8                       ; 0xc167c vgabios.c:1066
    xor ah, ah                                ; 30 e4                       ; 0xc167e
    xor dx, dx                                ; 31 d2                       ; 0xc1680
    call 01217h                               ; e8 92 fb                    ; 0xc1682
    inc cx                                    ; 41                          ; 0xc1685
    jmp short 01677h                          ; eb ef                       ; 0xc1686
    xor ax, ax                                ; 31 c0                       ; 0xc1688 vgabios.c:1069
    call 012a6h                               ; e8 19 fc                    ; 0xc168a
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc168d vgabios.c:1072
    xor bh, bh                                ; 30 ff                       ; 0xc1690
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1692
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1695
    jne short 016ach                          ; 75 10                       ; 0xc169a
    xor dx, dx                                ; 31 d2                       ; 0xc169c vgabios.c:1074
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc169e
    call 02cc1h                               ; e8 1d 16                    ; 0xc16a1
    xor bl, bl                                ; 30 db                       ; 0xc16a4 vgabios.c:1075
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc16a6
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc16a8
    int 06dh                                  ; cd 6d                       ; 0xc16aa
    mov bx, 0596ch                            ; bb 6c 59                    ; 0xc16ac vgabios.c:1079
    mov cx, ds                                ; 8c d9                       ; 0xc16af
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc16b1
    call 00980h                               ; e8 c9 f2                    ; 0xc16b4
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc16b7 vgabios.c:1081
    xor bh, bh                                ; 30 ff                       ; 0xc16ba
    sal bx, 006h                              ; c1 e3 06                    ; 0xc16bc
    mov dl, byte [bx+04844h]                  ; 8a 97 44 48                 ; 0xc16bf
    cmp dl, 010h                              ; 80 fa 10                    ; 0xc16c3
    je short 016e4h                           ; 74 1c                       ; 0xc16c6
    cmp dl, 00eh                              ; 80 fa 0e                    ; 0xc16c8
    je short 016dfh                           ; 74 12                       ; 0xc16cb
    cmp dl, 008h                              ; 80 fa 08                    ; 0xc16cd
    jne short 016e9h                          ; 75 17                       ; 0xc16d0
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc16d2 vgabios.c:1083
    mov cx, ds                                ; 8c d9                       ; 0xc16d5
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc16d7
    call 00980h                               ; e8 a3 f2                    ; 0xc16da
    jmp short 016e9h                          ; eb 0a                       ; 0xc16dd vgabios.c:1084
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc16df vgabios.c:1086
    jmp short 016d5h                          ; eb f1                       ; 0xc16e2
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc16e4 vgabios.c:1089
    jmp short 016d5h                          ; eb ec                       ; 0xc16e7
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc16e9 vgabios.c:1092
    pop di                                    ; 5f                          ; 0xc16ec
    pop si                                    ; 5e                          ; 0xc16ed
    pop dx                                    ; 5a                          ; 0xc16ee
    pop cx                                    ; 59                          ; 0xc16ef
    pop bx                                    ; 5b                          ; 0xc16f0
    pop bp                                    ; 5d                          ; 0xc16f1
    retn                                      ; c3                          ; 0xc16f2
  ; disGetNextSymbol 0xc16f3 LB 0x2b7c -> off=0x0 cb=000000000000008e uValue=00000000000c16f3 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc16f3 LB 0x8e
    push bp                                   ; 55                          ; 0xc16f3 vgabios.c:1095
    mov bp, sp                                ; 89 e5                       ; 0xc16f4
    push si                                   ; 56                          ; 0xc16f6
    push di                                   ; 57                          ; 0xc16f7
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc16f8
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc16fb
    mov al, dl                                ; 88 d0                       ; 0xc16fe
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1700
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1703
    xor ah, ah                                ; 30 e4                       ; 0xc1706 vgabios.c:1101
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1708
    xor dh, dh                                ; 30 f6                       ; 0xc170b
    mov cx, dx                                ; 89 d1                       ; 0xc170d
    imul dx                                   ; f7 ea                       ; 0xc170f
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1711
    xor dh, dh                                ; 30 f6                       ; 0xc1714
    mov si, dx                                ; 89 d6                       ; 0xc1716
    imul dx                                   ; f7 ea                       ; 0xc1718
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc171a
    xor dh, dh                                ; 30 f6                       ; 0xc171d
    mov bx, dx                                ; 89 d3                       ; 0xc171f
    add ax, dx                                ; 01 d0                       ; 0xc1721
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1723
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1726 vgabios.c:1102
    xor ah, ah                                ; 30 e4                       ; 0xc1729
    imul cx                                   ; f7 e9                       ; 0xc172b
    imul si                                   ; f7 ee                       ; 0xc172d
    add ax, bx                                ; 01 d8                       ; 0xc172f
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1731
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1734 vgabios.c:1103
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1737
    out DX, ax                                ; ef                          ; 0xc173a
    xor bl, bl                                ; 30 db                       ; 0xc173b vgabios.c:1104
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc173d
    jnc short 01771h                          ; 73 2f                       ; 0xc1740
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1742 vgabios.c:1106
    xor ah, ah                                ; 30 e4                       ; 0xc1745
    mov cx, ax                                ; 89 c1                       ; 0xc1747
    mov al, bl                                ; 88 d8                       ; 0xc1749
    mov dx, ax                                ; 89 c2                       ; 0xc174b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc174d
    mov si, ax                                ; 89 c6                       ; 0xc1750
    mov ax, dx                                ; 89 d0                       ; 0xc1752
    imul si                                   ; f7 ee                       ; 0xc1754
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1756
    add si, ax                                ; 01 c6                       ; 0xc1759
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc175b
    add di, ax                                ; 01 c7                       ; 0xc175e
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1760
    mov es, dx                                ; 8e c2                       ; 0xc1763
    jcxz 0176dh                               ; e3 06                       ; 0xc1765
    push DS                                   ; 1e                          ; 0xc1767
    mov ds, dx                                ; 8e da                       ; 0xc1768
    rep movsb                                 ; f3 a4                       ; 0xc176a
    pop DS                                    ; 1f                          ; 0xc176c
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc176d vgabios.c:1107
    jmp short 0173dh                          ; eb cc                       ; 0xc176f
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1771 vgabios.c:1108
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1774
    out DX, ax                                ; ef                          ; 0xc1777
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1778 vgabios.c:1109
    pop di                                    ; 5f                          ; 0xc177b
    pop si                                    ; 5e                          ; 0xc177c
    pop bp                                    ; 5d                          ; 0xc177d
    retn 00004h                               ; c2 04 00                    ; 0xc177e
  ; disGetNextSymbol 0xc1781 LB 0x2aee -> off=0x0 cb=000000000000007b uValue=00000000000c1781 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc1781 LB 0x7b
    push bp                                   ; 55                          ; 0xc1781 vgabios.c:1112
    mov bp, sp                                ; 89 e5                       ; 0xc1782
    push si                                   ; 56                          ; 0xc1784
    push di                                   ; 57                          ; 0xc1785
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1786
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1789
    mov al, dl                                ; 88 d0                       ; 0xc178c
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc178e
    mov bh, cl                                ; 88 cf                       ; 0xc1791
    xor ah, ah                                ; 30 e4                       ; 0xc1793 vgabios.c:1118
    mov dx, ax                                ; 89 c2                       ; 0xc1795
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1797
    mov cx, ax                                ; 89 c1                       ; 0xc179a
    mov ax, dx                                ; 89 d0                       ; 0xc179c
    imul cx                                   ; f7 e9                       ; 0xc179e
    mov dl, bh                                ; 88 fa                       ; 0xc17a0
    xor dh, dh                                ; 30 f6                       ; 0xc17a2
    imul dx                                   ; f7 ea                       ; 0xc17a4
    mov dx, ax                                ; 89 c2                       ; 0xc17a6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17a8
    xor ah, ah                                ; 30 e4                       ; 0xc17ab
    add dx, ax                                ; 01 c2                       ; 0xc17ad
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc17af
    mov ax, 00205h                            ; b8 05 02                    ; 0xc17b2 vgabios.c:1119
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17b5
    out DX, ax                                ; ef                          ; 0xc17b8
    xor bl, bl                                ; 30 db                       ; 0xc17b9 vgabios.c:1120
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc17bb
    jnc short 017ech                          ; 73 2c                       ; 0xc17be
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc17c0 vgabios.c:1122
    xor ch, ch                                ; 30 ed                       ; 0xc17c3
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc17c5
    xor ah, ah                                ; 30 e4                       ; 0xc17c8
    mov si, ax                                ; 89 c6                       ; 0xc17ca
    mov al, bl                                ; 88 d8                       ; 0xc17cc
    mov dx, ax                                ; 89 c2                       ; 0xc17ce
    mov al, bh                                ; 88 f8                       ; 0xc17d0
    mov di, ax                                ; 89 c7                       ; 0xc17d2
    mov ax, dx                                ; 89 d0                       ; 0xc17d4
    imul di                                   ; f7 ef                       ; 0xc17d6
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17d8
    add di, ax                                ; 01 c7                       ; 0xc17db
    mov ax, si                                ; 89 f0                       ; 0xc17dd
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc17df
    mov es, dx                                ; 8e c2                       ; 0xc17e2
    jcxz 017e8h                               ; e3 02                       ; 0xc17e4
    rep stosb                                 ; f3 aa                       ; 0xc17e6
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc17e8 vgabios.c:1123
    jmp short 017bbh                          ; eb cf                       ; 0xc17ea
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc17ec vgabios.c:1124
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17ef
    out DX, ax                                ; ef                          ; 0xc17f2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17f3 vgabios.c:1125
    pop di                                    ; 5f                          ; 0xc17f6
    pop si                                    ; 5e                          ; 0xc17f7
    pop bp                                    ; 5d                          ; 0xc17f8
    retn 00004h                               ; c2 04 00                    ; 0xc17f9
  ; disGetNextSymbol 0xc17fc LB 0x2a73 -> off=0x0 cb=00000000000000b6 uValue=00000000000c17fc 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc17fc LB 0xb6
    push bp                                   ; 55                          ; 0xc17fc vgabios.c:1128
    mov bp, sp                                ; 89 e5                       ; 0xc17fd
    push si                                   ; 56                          ; 0xc17ff
    push di                                   ; 57                          ; 0xc1800
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1801
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1804
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1807
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc180a
    mov al, dl                                ; 88 d0                       ; 0xc180d vgabios.c:1134
    xor ah, ah                                ; 30 e4                       ; 0xc180f
    mov bx, ax                                ; 89 c3                       ; 0xc1811
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1813
    mov si, ax                                ; 89 c6                       ; 0xc1816
    mov ax, bx                                ; 89 d8                       ; 0xc1818
    imul si                                   ; f7 ee                       ; 0xc181a
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc181c
    mov di, bx                                ; 89 df                       ; 0xc181f
    imul bx                                   ; f7 eb                       ; 0xc1821
    mov dx, ax                                ; 89 c2                       ; 0xc1823
    sar dx, 1                                 ; d1 fa                       ; 0xc1825
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1827
    xor ah, ah                                ; 30 e4                       ; 0xc182a
    mov bx, ax                                ; 89 c3                       ; 0xc182c
    add dx, ax                                ; 01 c2                       ; 0xc182e
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1830
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1833 vgabios.c:1135
    imul si                                   ; f7 ee                       ; 0xc1836
    imul di                                   ; f7 ef                       ; 0xc1838
    sar ax, 1                                 ; d1 f8                       ; 0xc183a
    add ax, bx                                ; 01 d8                       ; 0xc183c
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc183e
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc1841 vgabios.c:1136
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1844
    xor ah, ah                                ; 30 e4                       ; 0xc1847
    cwd                                       ; 99                          ; 0xc1849
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc184a
    sar ax, 1                                 ; d1 f8                       ; 0xc184c
    mov bx, ax                                ; 89 c3                       ; 0xc184e
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1850
    xor ah, ah                                ; 30 e4                       ; 0xc1853
    cmp ax, bx                                ; 39 d8                       ; 0xc1855
    jnl short 018a9h                          ; 7d 50                       ; 0xc1857
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1859 vgabios.c:1138
    xor bh, bh                                ; 30 ff                       ; 0xc185c
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc185e
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1861
    imul bx                                   ; f7 eb                       ; 0xc1864
    mov bx, ax                                ; 89 c3                       ; 0xc1866
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1868
    add si, ax                                ; 01 c6                       ; 0xc186b
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc186d
    add di, ax                                ; 01 c7                       ; 0xc1870
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1872
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1875
    mov es, dx                                ; 8e c2                       ; 0xc1878
    jcxz 01882h                               ; e3 06                       ; 0xc187a
    push DS                                   ; 1e                          ; 0xc187c
    mov ds, dx                                ; 8e da                       ; 0xc187d
    rep movsb                                 ; f3 a4                       ; 0xc187f
    pop DS                                    ; 1f                          ; 0xc1881
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1882 vgabios.c:1139
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc1885
    add si, bx                                ; 01 de                       ; 0xc1889
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc188b
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc188e
    add di, bx                                ; 01 df                       ; 0xc1892
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1894
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1897
    mov es, dx                                ; 8e c2                       ; 0xc189a
    jcxz 018a4h                               ; e3 06                       ; 0xc189c
    push DS                                   ; 1e                          ; 0xc189e
    mov ds, dx                                ; 8e da                       ; 0xc189f
    rep movsb                                 ; f3 a4                       ; 0xc18a1
    pop DS                                    ; 1f                          ; 0xc18a3
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18a4 vgabios.c:1140
    jmp short 01844h                          ; eb 9b                       ; 0xc18a7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18a9 vgabios.c:1141
    pop di                                    ; 5f                          ; 0xc18ac
    pop si                                    ; 5e                          ; 0xc18ad
    pop bp                                    ; 5d                          ; 0xc18ae
    retn 00004h                               ; c2 04 00                    ; 0xc18af
  ; disGetNextSymbol 0xc18b2 LB 0x29bd -> off=0x0 cb=0000000000000094 uValue=00000000000c18b2 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc18b2 LB 0x94
    push bp                                   ; 55                          ; 0xc18b2 vgabios.c:1144
    mov bp, sp                                ; 89 e5                       ; 0xc18b3
    push si                                   ; 56                          ; 0xc18b5
    push di                                   ; 57                          ; 0xc18b6
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc18b7
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc18ba
    mov al, dl                                ; 88 d0                       ; 0xc18bd
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc18bf
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc18c2
    xor ah, ah                                ; 30 e4                       ; 0xc18c5 vgabios.c:1150
    mov dx, ax                                ; 89 c2                       ; 0xc18c7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc18c9
    mov bx, ax                                ; 89 c3                       ; 0xc18cc
    mov ax, dx                                ; 89 d0                       ; 0xc18ce
    imul bx                                   ; f7 eb                       ; 0xc18d0
    mov dl, cl                                ; 88 ca                       ; 0xc18d2
    xor dh, dh                                ; 30 f6                       ; 0xc18d4
    imul dx                                   ; f7 ea                       ; 0xc18d6
    mov dx, ax                                ; 89 c2                       ; 0xc18d8
    sar dx, 1                                 ; d1 fa                       ; 0xc18da
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc18dc
    xor ah, ah                                ; 30 e4                       ; 0xc18df
    add dx, ax                                ; 01 c2                       ; 0xc18e1
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc18e3
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc18e6 vgabios.c:1151
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc18e9
    xor ah, ah                                ; 30 e4                       ; 0xc18ec
    cwd                                       ; 99                          ; 0xc18ee
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc18ef
    sar ax, 1                                 ; d1 f8                       ; 0xc18f1
    mov dx, ax                                ; 89 c2                       ; 0xc18f3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18f5
    xor ah, ah                                ; 30 e4                       ; 0xc18f8
    cmp ax, dx                                ; 39 d0                       ; 0xc18fa
    jnl short 0193dh                          ; 7d 3f                       ; 0xc18fc
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc18fe vgabios.c:1153
    xor bh, bh                                ; 30 ff                       ; 0xc1901
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1903
    xor dh, dh                                ; 30 f6                       ; 0xc1906
    mov si, dx                                ; 89 d6                       ; 0xc1908
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc190a
    imul dx                                   ; f7 ea                       ; 0xc190d
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc190f
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1912
    add di, ax                                ; 01 c7                       ; 0xc1915
    mov cx, bx                                ; 89 d9                       ; 0xc1917
    mov ax, si                                ; 89 f0                       ; 0xc1919
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc191b
    mov es, dx                                ; 8e c2                       ; 0xc191e
    jcxz 01924h                               ; e3 02                       ; 0xc1920
    rep stosb                                 ; f3 aa                       ; 0xc1922
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1924 vgabios.c:1154
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1927
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc192b
    mov cx, bx                                ; 89 d9                       ; 0xc192e
    mov ax, si                                ; 89 f0                       ; 0xc1930
    mov es, dx                                ; 8e c2                       ; 0xc1932
    jcxz 01938h                               ; e3 02                       ; 0xc1934
    rep stosb                                 ; f3 aa                       ; 0xc1936
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1938 vgabios.c:1155
    jmp short 018e9h                          ; eb ac                       ; 0xc193b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc193d vgabios.c:1156
    pop di                                    ; 5f                          ; 0xc1940
    pop si                                    ; 5e                          ; 0xc1941
    pop bp                                    ; 5d                          ; 0xc1942
    retn 00004h                               ; c2 04 00                    ; 0xc1943
  ; disGetNextSymbol 0xc1946 LB 0x2929 -> off=0x0 cb=0000000000000081 uValue=00000000000c1946 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1946 LB 0x81
    push bp                                   ; 55                          ; 0xc1946 vgabios.c:1159
    mov bp, sp                                ; 89 e5                       ; 0xc1947
    push si                                   ; 56                          ; 0xc1949
    push di                                   ; 57                          ; 0xc194a
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc194b
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc194e
    mov al, dl                                ; 88 d0                       ; 0xc1951
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1953
    mov bx, cx                                ; 89 cb                       ; 0xc1956
    xor ah, ah                                ; 30 e4                       ; 0xc1958 vgabios.c:1165
    mov si, ax                                ; 89 c6                       ; 0xc195a
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc195c
    mov di, ax                                ; 89 c7                       ; 0xc195f
    mov ax, si                                ; 89 f0                       ; 0xc1961
    imul di                                   ; f7 ef                       ; 0xc1963
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1965
    mov si, ax                                ; 89 c6                       ; 0xc1968
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc196a
    xor ah, ah                                ; 30 e4                       ; 0xc196d
    mov cx, ax                                ; 89 c1                       ; 0xc196f
    add si, ax                                ; 01 c6                       ; 0xc1971
    sal si, 003h                              ; c1 e6 03                    ; 0xc1973
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1976
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1979 vgabios.c:1166
    imul di                                   ; f7 ef                       ; 0xc197c
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc197e
    add ax, cx                                ; 01 c8                       ; 0xc1981
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1983
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1986
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1989 vgabios.c:1167
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc198c vgabios.c:1168
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc1990 vgabios.c:1169
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1993
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc1996
    jnc short 019beh                          ; 73 23                       ; 0xc1999
    xor ah, ah                                ; 30 e4                       ; 0xc199b vgabios.c:1171
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc199d
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc19a0
    add si, ax                                ; 01 c6                       ; 0xc19a3
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc19a5
    add di, ax                                ; 01 c7                       ; 0xc19a8
    mov cx, bx                                ; 89 d9                       ; 0xc19aa
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc19ac
    mov es, dx                                ; 8e c2                       ; 0xc19af
    jcxz 019b9h                               ; e3 06                       ; 0xc19b1
    push DS                                   ; 1e                          ; 0xc19b3
    mov ds, dx                                ; 8e da                       ; 0xc19b4
    rep movsb                                 ; f3 a4                       ; 0xc19b6
    pop DS                                    ; 1f                          ; 0xc19b8
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc19b9 vgabios.c:1172
    jmp short 01993h                          ; eb d5                       ; 0xc19bc
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc19be vgabios.c:1173
    pop di                                    ; 5f                          ; 0xc19c1
    pop si                                    ; 5e                          ; 0xc19c2
    pop bp                                    ; 5d                          ; 0xc19c3
    retn 00004h                               ; c2 04 00                    ; 0xc19c4
  ; disGetNextSymbol 0xc19c7 LB 0x28a8 -> off=0x0 cb=000000000000006d uValue=00000000000c19c7 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc19c7 LB 0x6d
    push bp                                   ; 55                          ; 0xc19c7 vgabios.c:1176
    mov bp, sp                                ; 89 e5                       ; 0xc19c8
    push si                                   ; 56                          ; 0xc19ca
    push di                                   ; 57                          ; 0xc19cb
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc19cc
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc19cf
    mov al, dl                                ; 88 d0                       ; 0xc19d2
    mov si, cx                                ; 89 ce                       ; 0xc19d4
    xor ah, ah                                ; 30 e4                       ; 0xc19d6 vgabios.c:1182
    mov dx, ax                                ; 89 c2                       ; 0xc19d8
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc19da
    mov di, ax                                ; 89 c7                       ; 0xc19dd
    mov ax, dx                                ; 89 d0                       ; 0xc19df
    imul di                                   ; f7 ef                       ; 0xc19e1
    mul cx                                    ; f7 e1                       ; 0xc19e3
    mov dx, ax                                ; 89 c2                       ; 0xc19e5
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc19e7
    xor ah, ah                                ; 30 e4                       ; 0xc19ea
    add ax, dx                                ; 01 d0                       ; 0xc19ec
    sal ax, 003h                              ; c1 e0 03                    ; 0xc19ee
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc19f1
    sal bx, 003h                              ; c1 e3 03                    ; 0xc19f4 vgabios.c:1183
    sal si, 003h                              ; c1 e6 03                    ; 0xc19f7 vgabios.c:1184
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc19fa vgabios.c:1185
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19fe
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1a01
    jnc short 01a2bh                          ; 73 25                       ; 0xc1a04
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a06 vgabios.c:1187
    xor ah, ah                                ; 30 e4                       ; 0xc1a09
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1a0b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a0e
    mul si                                    ; f7 e6                       ; 0xc1a11
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1a13
    add di, ax                                ; 01 c7                       ; 0xc1a16
    mov cx, bx                                ; 89 d9                       ; 0xc1a18
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1a1a
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a1d
    mov es, dx                                ; 8e c2                       ; 0xc1a20
    jcxz 01a26h                               ; e3 02                       ; 0xc1a22
    rep stosb                                 ; f3 aa                       ; 0xc1a24
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1a26 vgabios.c:1188
    jmp short 019feh                          ; eb d3                       ; 0xc1a29
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a2b vgabios.c:1189
    pop di                                    ; 5f                          ; 0xc1a2e
    pop si                                    ; 5e                          ; 0xc1a2f
    pop bp                                    ; 5d                          ; 0xc1a30
    retn 00004h                               ; c2 04 00                    ; 0xc1a31
  ; disGetNextSymbol 0xc1a34 LB 0x283b -> off=0x0 cb=0000000000000688 uValue=00000000000c1a34 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1a34 LB 0x688
    push bp                                   ; 55                          ; 0xc1a34 vgabios.c:1192
    mov bp, sp                                ; 89 e5                       ; 0xc1a35
    push si                                   ; 56                          ; 0xc1a37
    push di                                   ; 57                          ; 0xc1a38
    sub sp, strict byte 0001eh                ; 83 ec 1e                    ; 0xc1a39
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a3c
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1a3f
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1a42
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1a45
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1a48 vgabios.c:1201
    jnbe short 01a69h                         ; 77 1c                       ; 0xc1a4b
    cmp cl, byte [bp+006h]                    ; 3a 4e 06                    ; 0xc1a4d vgabios.c:1202
    jnbe short 01a69h                         ; 77 17                       ; 0xc1a50
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1a52 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1a55
    mov es, ax                                ; 8e c0                       ; 0xc1a58
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a5a
    xor ah, ah                                ; 30 e4                       ; 0xc1a5d vgabios.c:1206
    call 035b3h                               ; e8 51 1b                    ; 0xc1a5f
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1a62
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1a65 vgabios.c:1207
    jne short 01a6ch                          ; 75 03                       ; 0xc1a67
    jmp near 020b3h                           ; e9 47 06                    ; 0xc1a69
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1a6c vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1a6f
    mov es, ax                                ; 8e c0                       ; 0xc1a72
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a74
    xor ah, ah                                ; 30 e4                       ; 0xc1a77 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc1a79
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1a7a
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1a7d vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1a80
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1a83 vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1a86 vgabios.c:1214
    jne short 01a95h                          ; 75 09                       ; 0xc1a8a
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1a8c vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a8f
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1a92 vgabios.c:38
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a95 vgabios.c:1217
    xor ah, ah                                ; 30 e4                       ; 0xc1a98
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a9a
    jc short 01aa7h                           ; 72 08                       ; 0xc1a9d
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a9f
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1aa2
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1aa4
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1aa7 vgabios.c:1218
    xor ah, ah                                ; 30 e4                       ; 0xc1aaa
    cmp ax, word [bp-01eh]                    ; 3b 46 e2                    ; 0xc1aac
    jc short 01ab9h                           ; 72 08                       ; 0xc1aaf
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1ab1
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ab4
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc1ab6
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ab9 vgabios.c:1219
    xor ah, ah                                ; 30 e4                       ; 0xc1abc
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1abe
    jbe short 01ac6h                          ; 76 03                       ; 0xc1ac1
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1ac3
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1ac6 vgabios.c:1220
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1ac9
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1acc
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1ace
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1ad1 vgabios.c:1222
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1ad4
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1ad7
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1adb
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1ade
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1ae1
    dec ax                                    ; 48                          ; 0xc1ae4
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1ae5
    mov di, word [bp-016h]                    ; 8b 7e ea                    ; 0xc1ae8
    dec di                                    ; 4f                          ; 0xc1aeb
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1aec
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc1aef
    mov cx, ax                                ; 89 c1                       ; 0xc1af2
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1af4
    jne short 01b44h                          ; 75 49                       ; 0xc1af9
    add ax, ax                                ; 01 c0                       ; 0xc1afb vgabios.c:1225
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1afd
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1aff
    xor dh, dh                                ; 30 f6                       ; 0xc1b02
    inc ax                                    ; 40                          ; 0xc1b04
    mul dx                                    ; f7 e2                       ; 0xc1b05
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1b07
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b0a vgabios.c:1230
    jne short 01b47h                          ; 75 37                       ; 0xc1b0e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1b10
    jne short 01b47h                          ; 75 31                       ; 0xc1b14
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b16
    jne short 01b47h                          ; 75 2b                       ; 0xc1b1a
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b1c
    xor ah, ah                                ; 30 e4                       ; 0xc1b1f
    cmp ax, di                                ; 39 f8                       ; 0xc1b21
    jne short 01b47h                          ; 75 22                       ; 0xc1b23
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1b25
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1b28
    jne short 01b47h                          ; 75 1a                       ; 0xc1b2b
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b2d vgabios.c:1232
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1b30
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b33
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b36
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1b3a
    jcxz 01b41h                               ; e3 02                       ; 0xc1b3d
    rep stosw                                 ; f3 ab                       ; 0xc1b3f
    jmp near 020b3h                           ; e9 6f 05                    ; 0xc1b41 vgabios.c:1234
    jmp near 01cb7h                           ; e9 70 01                    ; 0xc1b44
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1b47 vgabios.c:1236
    jne short 01badh                          ; 75 60                       ; 0xc1b4b
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1b4d vgabios.c:1237
    xor ah, ah                                ; 30 e4                       ; 0xc1b50
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1b52
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1b55
    xor dh, dh                                ; 30 f6                       ; 0xc1b58
    cmp dx, word [bp-01ch]                    ; 3b 56 e4                    ; 0xc1b5a
    jc short 01bafh                           ; 72 50                       ; 0xc1b5d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b5f vgabios.c:1239
    xor ah, ah                                ; 30 e4                       ; 0xc1b62
    add ax, word [bp-01ch]                    ; 03 46 e4                    ; 0xc1b64
    cmp ax, dx                                ; 39 d0                       ; 0xc1b67
    jnbe short 01b71h                         ; 77 06                       ; 0xc1b69
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b6b
    jne short 01bb2h                          ; 75 41                       ; 0xc1b6f
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1b71 vgabios.c:1240
    xor ch, ch                                ; 30 ed                       ; 0xc1b74
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b76
    xor ah, ah                                ; 30 e4                       ; 0xc1b79
    mov si, ax                                ; 89 c6                       ; 0xc1b7b
    sal si, 008h                              ; c1 e6 08                    ; 0xc1b7d
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1b80
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1b83
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1b86
    mov dx, ax                                ; 89 c2                       ; 0xc1b89
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b8b
    xor ah, ah                                ; 30 e4                       ; 0xc1b8e
    mov di, ax                                ; 89 c7                       ; 0xc1b90
    add di, dx                                ; 01 d7                       ; 0xc1b92
    add di, di                                ; 01 ff                       ; 0xc1b94
    add di, word [bp-020h]                    ; 03 7e e0                    ; 0xc1b96
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1b99
    xor bh, bh                                ; 30 ff                       ; 0xc1b9c
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b9e
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1ba1
    mov ax, si                                ; 89 f0                       ; 0xc1ba5
    jcxz 01babh                               ; e3 02                       ; 0xc1ba7
    rep stosw                                 ; f3 ab                       ; 0xc1ba9
    jmp short 01bf2h                          ; eb 45                       ; 0xc1bab vgabios.c:1241
    jmp short 01bf8h                          ; eb 49                       ; 0xc1bad
    jmp near 020b3h                           ; e9 01 05                    ; 0xc1baf
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1bb2 vgabios.c:1242
    xor ch, ch                                ; 30 ed                       ; 0xc1bb5
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1bb7
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1bba
    mov byte [bp-018h], dl                    ; 88 56 e8                    ; 0xc1bbd
    mov byte [bp-017h], ch                    ; 88 6e e9                    ; 0xc1bc0
    mov si, ax                                ; 89 c6                       ; 0xc1bc3
    add si, word [bp-018h]                    ; 03 76 e8                    ; 0xc1bc5
    add si, si                                ; 01 f6                       ; 0xc1bc8
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1bca
    xor bh, bh                                ; 30 ff                       ; 0xc1bcd
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1bcf
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1bd2
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1bd6
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1bd9
    add ax, word [bp-018h]                    ; 03 46 e8                    ; 0xc1bdc
    add ax, ax                                ; 01 c0                       ; 0xc1bdf
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1be1
    add di, ax                                ; 01 c7                       ; 0xc1be4
    mov dx, bx                                ; 89 da                       ; 0xc1be6
    mov es, bx                                ; 8e c3                       ; 0xc1be8
    jcxz 01bf2h                               ; e3 06                       ; 0xc1bea
    push DS                                   ; 1e                          ; 0xc1bec
    mov ds, dx                                ; 8e da                       ; 0xc1bed
    rep movsw                                 ; f3 a5                       ; 0xc1bef
    pop DS                                    ; 1f                          ; 0xc1bf1
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1bf2 vgabios.c:1243
    jmp near 01b55h                           ; e9 5d ff                    ; 0xc1bf5
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1bf8 vgabios.c:1246
    xor ah, ah                                ; 30 e4                       ; 0xc1bfb
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1bfd
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1c00
    xor ah, ah                                ; 30 e4                       ; 0xc1c03
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1c05
    jnbe short 01bafh                         ; 77 a5                       ; 0xc1c08
    mov dl, al                                ; 88 c2                       ; 0xc1c0a vgabios.c:1248
    xor dh, dh                                ; 30 f6                       ; 0xc1c0c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c0e
    add ax, dx                                ; 01 d0                       ; 0xc1c11
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1c13
    jnbe short 01c1eh                         ; 77 06                       ; 0xc1c16
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1c18
    jne short 01c5ah                          ; 75 3c                       ; 0xc1c1c
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1c1e vgabios.c:1249
    xor ch, ch                                ; 30 ed                       ; 0xc1c21
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1c23
    xor ah, ah                                ; 30 e4                       ; 0xc1c26
    mov si, ax                                ; 89 c6                       ; 0xc1c28
    sal si, 008h                              ; c1 e6 08                    ; 0xc1c2a
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1c2d
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1c30
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c33
    mov dx, ax                                ; 89 c2                       ; 0xc1c36
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1c38
    xor ah, ah                                ; 30 e4                       ; 0xc1c3b
    add ax, dx                                ; 01 d0                       ; 0xc1c3d
    add ax, ax                                ; 01 c0                       ; 0xc1c3f
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1c41
    add di, ax                                ; 01 c7                       ; 0xc1c44
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c46
    xor bh, bh                                ; 30 ff                       ; 0xc1c49
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c4b
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1c4e
    mov ax, si                                ; 89 f0                       ; 0xc1c52
    jcxz 01c58h                               ; e3 02                       ; 0xc1c54
    rep stosw                                 ; f3 ab                       ; 0xc1c56
    jmp short 01ca7h                          ; eb 4d                       ; 0xc1c58 vgabios.c:1250
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1c5a vgabios.c:1251
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1c5d
    mov byte [bp-017h], dh                    ; 88 76 e9                    ; 0xc1c60
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c63
    xor ah, ah                                ; 30 e4                       ; 0xc1c66
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1c68
    sub dx, ax                                ; 29 c2                       ; 0xc1c6b
    mov ax, dx                                ; 89 d0                       ; 0xc1c6d
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c6f
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc1c72
    xor ch, ch                                ; 30 ed                       ; 0xc1c75
    mov si, ax                                ; 89 c6                       ; 0xc1c77
    add si, cx                                ; 01 ce                       ; 0xc1c79
    add si, si                                ; 01 f6                       ; 0xc1c7b
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c7d
    xor bh, bh                                ; 30 ff                       ; 0xc1c80
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c82
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1c85
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1c89
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c8c
    add ax, cx                                ; 01 c8                       ; 0xc1c8f
    add ax, ax                                ; 01 c0                       ; 0xc1c91
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1c93
    add di, ax                                ; 01 c7                       ; 0xc1c96
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc1c98
    mov dx, bx                                ; 89 da                       ; 0xc1c9b
    mov es, bx                                ; 8e c3                       ; 0xc1c9d
    jcxz 01ca7h                               ; e3 06                       ; 0xc1c9f
    push DS                                   ; 1e                          ; 0xc1ca1
    mov ds, dx                                ; 8e da                       ; 0xc1ca2
    rep movsw                                 ; f3 a5                       ; 0xc1ca4
    pop DS                                    ; 1f                          ; 0xc1ca6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ca7 vgabios.c:1252
    xor ah, ah                                ; 30 e4                       ; 0xc1caa
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1cac
    jc short 01ce4h                           ; 72 33                       ; 0xc1caf
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1cb1 vgabios.c:1253
    jmp near 01c00h                           ; e9 49 ff                    ; 0xc1cb4
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1cb7 vgabios.c:1259
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1cba
    xor ah, ah                                ; 30 e4                       ; 0xc1cbe
    mov si, ax                                ; 89 c6                       ; 0xc1cc0
    sal si, 006h                              ; c1 e6 06                    ; 0xc1cc2
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1cc5
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1cc9
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1ccc vgabios.c:1260
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1cd0
    jc short 01ce0h                           ; 72 0c                       ; 0xc1cd2
    jbe short 01ce7h                          ; 76 11                       ; 0xc1cd4
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1cd6
    je short 01d15h                           ; 74 3b                       ; 0xc1cd8
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1cda
    je short 01ce7h                           ; 74 09                       ; 0xc1cdc
    jmp short 01ce4h                          ; eb 04                       ; 0xc1cde
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1ce0
    je short 01d18h                           ; 74 34                       ; 0xc1ce2
    jmp near 020b3h                           ; e9 cc 03                    ; 0xc1ce4
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ce7 vgabios.c:1264
    jne short 01d13h                          ; 75 26                       ; 0xc1ceb
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1ced
    jne short 01d55h                          ; 75 62                       ; 0xc1cf1
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1cf3
    jne short 01d55h                          ; 75 5c                       ; 0xc1cf7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1cf9
    xor ah, ah                                ; 30 e4                       ; 0xc1cfc
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1cfe
    dec dx                                    ; 4a                          ; 0xc1d01
    cmp ax, dx                                ; 39 d0                       ; 0xc1d02
    jne short 01d55h                          ; 75 4f                       ; 0xc1d04
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1d06
    xor ah, dh                                ; 30 f4                       ; 0xc1d09
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc1d0b
    dec dx                                    ; 4a                          ; 0xc1d0e
    cmp ax, dx                                ; 39 d0                       ; 0xc1d0f
    je short 01d1bh                           ; 74 08                       ; 0xc1d11
    jmp short 01d55h                          ; eb 40                       ; 0xc1d13
    jmp near 01f8bh                           ; e9 73 02                    ; 0xc1d15
    jmp near 01e45h                           ; e9 2a 01                    ; 0xc1d18
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1d1b vgabios.c:1266
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d1e
    out DX, ax                                ; ef                          ; 0xc1d21
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1d22 vgabios.c:1267
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1d25
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1d28
    xor dh, dh                                ; 30 f6                       ; 0xc1d2b
    mul dx                                    ; f7 e2                       ; 0xc1d2d
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc1d2f
    xor dh, dh                                ; 30 f6                       ; 0xc1d32
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1d34
    xor bh, bh                                ; 30 ff                       ; 0xc1d37
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1d39
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1d3c
    mov cx, ax                                ; 89 c1                       ; 0xc1d40
    mov ax, dx                                ; 89 d0                       ; 0xc1d42
    xor di, di                                ; 31 ff                       ; 0xc1d44
    mov es, bx                                ; 8e c3                       ; 0xc1d46
    jcxz 01d4ch                               ; e3 02                       ; 0xc1d48
    rep stosb                                 ; f3 aa                       ; 0xc1d4a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1d4c vgabios.c:1268
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d4f
    out DX, ax                                ; ef                          ; 0xc1d52
    jmp short 01ce4h                          ; eb 8f                       ; 0xc1d53 vgabios.c:1270
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d55 vgabios.c:1272
    jne short 01dd0h                          ; 75 75                       ; 0xc1d59
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1d5b vgabios.c:1273
    xor ah, ah                                ; 30 e4                       ; 0xc1d5e
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1d60
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d63
    xor ah, ah                                ; 30 e4                       ; 0xc1d66
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1d68
    jc short 01dcdh                           ; 72 60                       ; 0xc1d6b
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1d6d vgabios.c:1275
    xor dh, dh                                ; 30 f6                       ; 0xc1d70
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1d72
    cmp dx, ax                                ; 39 c2                       ; 0xc1d75
    jnbe short 01d7fh                         ; 77 06                       ; 0xc1d77
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d79
    jne short 01da0h                          ; 75 21                       ; 0xc1d7d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1d7f vgabios.c:1276
    xor ah, ah                                ; 30 e4                       ; 0xc1d82
    push ax                                   ; 50                          ; 0xc1d84
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1d85
    push ax                                   ; 50                          ; 0xc1d88
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1d89
    xor ch, ch                                ; 30 ed                       ; 0xc1d8c
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1d8e
    xor bh, bh                                ; 30 ff                       ; 0xc1d91
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1d93
    xor dh, dh                                ; 30 f6                       ; 0xc1d96
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1d98
    call 01781h                               ; e8 e3 f9                    ; 0xc1d9b
    jmp short 01dc8h                          ; eb 28                       ; 0xc1d9e vgabios.c:1277
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1da0 vgabios.c:1278
    push ax                                   ; 50                          ; 0xc1da3
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1da4
    push ax                                   ; 50                          ; 0xc1da7
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1da8
    xor ch, ch                                ; 30 ed                       ; 0xc1dab
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1dad
    xor bh, bh                                ; 30 ff                       ; 0xc1db0
    mov dl, bl                                ; 88 da                       ; 0xc1db2
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1db4
    xor dh, dh                                ; 30 f6                       ; 0xc1db7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1db9
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1dbc
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc1dbf
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1dc2
    call 016f3h                               ; e8 2b f9                    ; 0xc1dc5
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1dc8 vgabios.c:1279
    jmp short 01d63h                          ; eb 96                       ; 0xc1dcb
    jmp near 020b3h                           ; e9 e3 02                    ; 0xc1dcd
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1dd0 vgabios.c:1282
    xor ah, ah                                ; 30 e4                       ; 0xc1dd3
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1dd5
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1dd8
    xor ah, ah                                ; 30 e4                       ; 0xc1ddb
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1ddd
    jnbe short 01dcdh                         ; 77 eb                       ; 0xc1de0
    mov dl, al                                ; 88 c2                       ; 0xc1de2 vgabios.c:1284
    xor dh, dh                                ; 30 f6                       ; 0xc1de4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1de6
    add ax, dx                                ; 01 d0                       ; 0xc1de9
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1deb
    jnbe short 01df6h                         ; 77 06                       ; 0xc1dee
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1df0
    jne short 01e17h                          ; 75 21                       ; 0xc1df4
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1df6 vgabios.c:1285
    xor ah, ah                                ; 30 e4                       ; 0xc1df9
    push ax                                   ; 50                          ; 0xc1dfb
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1dfc
    push ax                                   ; 50                          ; 0xc1dff
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1e00
    xor ch, ch                                ; 30 ed                       ; 0xc1e03
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e05
    xor bh, bh                                ; 30 ff                       ; 0xc1e08
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1e0a
    xor dh, dh                                ; 30 f6                       ; 0xc1e0d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e0f
    call 01781h                               ; e8 6c f9                    ; 0xc1e12
    jmp short 01e36h                          ; eb 1f                       ; 0xc1e15 vgabios.c:1286
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e17 vgabios.c:1287
    xor ah, ah                                ; 30 e4                       ; 0xc1e1a
    push ax                                   ; 50                          ; 0xc1e1c
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1e1d
    push ax                                   ; 50                          ; 0xc1e20
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e21
    xor ch, ch                                ; 30 ed                       ; 0xc1e24
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1e26
    xor bh, bh                                ; 30 ff                       ; 0xc1e29
    mov dl, bl                                ; 88 da                       ; 0xc1e2b
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1e2d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e30
    call 016f3h                               ; e8 bd f8                    ; 0xc1e33
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e36 vgabios.c:1288
    xor ah, ah                                ; 30 e4                       ; 0xc1e39
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1e3b
    jc short 01e8eh                           ; 72 4e                       ; 0xc1e3e
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1e40 vgabios.c:1289
    jmp short 01dd8h                          ; eb 93                       ; 0xc1e43
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc1e45 vgabios.c:1294
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1e49
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1e4c vgabios.c:1295
    jne short 01e91h                          ; 75 3f                       ; 0xc1e50
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1e52
    jne short 01e91h                          ; 75 39                       ; 0xc1e56
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1e58
    jne short 01e91h                          ; 75 33                       ; 0xc1e5c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e5e
    cmp ax, di                                ; 39 f8                       ; 0xc1e61
    jne short 01e91h                          ; 75 2c                       ; 0xc1e63
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1e65
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1e68
    jne short 01e91h                          ; 75 24                       ; 0xc1e6b
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1e6d vgabios.c:1297
    xor dh, dh                                ; 30 f6                       ; 0xc1e70
    mov ax, cx                                ; 89 c8                       ; 0xc1e72
    mul dx                                    ; f7 e2                       ; 0xc1e74
    mov dl, byte [bp-014h]                    ; 8a 56 ec                    ; 0xc1e76
    xor dh, dh                                ; 30 f6                       ; 0xc1e79
    mul dx                                    ; f7 e2                       ; 0xc1e7b
    mov cx, ax                                ; 89 c1                       ; 0xc1e7d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e7f
    xor ah, ah                                ; 30 e4                       ; 0xc1e82
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1e84
    xor di, di                                ; 31 ff                       ; 0xc1e88
    jcxz 01e8eh                               ; e3 02                       ; 0xc1e8a
    rep stosb                                 ; f3 aa                       ; 0xc1e8c
    jmp near 020b3h                           ; e9 22 02                    ; 0xc1e8e vgabios.c:1299
    cmp byte [bp-014h], 002h                  ; 80 7e ec 02                 ; 0xc1e91 vgabios.c:1301
    jne short 01ea0h                          ; 75 09                       ; 0xc1e95
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc1e97 vgabios.c:1303
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1e9a vgabios.c:1304
    sal word [bp-01eh], 1                     ; d1 66 e2                    ; 0xc1e9d vgabios.c:1305
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1ea0 vgabios.c:1308
    jne short 01f0fh                          ; 75 69                       ; 0xc1ea4
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1ea6 vgabios.c:1309
    xor ah, ah                                ; 30 e4                       ; 0xc1ea9
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1eab
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1eae
    xor ah, ah                                ; 30 e4                       ; 0xc1eb1
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1eb3
    jc short 01e8eh                           ; 72 d6                       ; 0xc1eb6
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1eb8 vgabios.c:1311
    xor dh, dh                                ; 30 f6                       ; 0xc1ebb
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1ebd
    cmp dx, ax                                ; 39 c2                       ; 0xc1ec0
    jnbe short 01ecah                         ; 77 06                       ; 0xc1ec2
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ec4
    jne short 01eebh                          ; 75 21                       ; 0xc1ec8
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1eca vgabios.c:1312
    xor ah, ah                                ; 30 e4                       ; 0xc1ecd
    push ax                                   ; 50                          ; 0xc1ecf
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1ed0
    push ax                                   ; 50                          ; 0xc1ed3
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1ed4
    xor ch, ch                                ; 30 ed                       ; 0xc1ed7
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1ed9
    xor bh, bh                                ; 30 ff                       ; 0xc1edc
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1ede
    xor dh, dh                                ; 30 f6                       ; 0xc1ee1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ee3
    call 018b2h                               ; e8 c9 f9                    ; 0xc1ee6
    jmp short 01f0ah                          ; eb 1f                       ; 0xc1ee9 vgabios.c:1313
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1eeb vgabios.c:1314
    push ax                                   ; 50                          ; 0xc1eee
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1eef
    push ax                                   ; 50                          ; 0xc1ef2
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1ef3
    xor ch, ch                                ; 30 ed                       ; 0xc1ef6
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1ef8
    xor bh, bh                                ; 30 ff                       ; 0xc1efb
    mov dl, bl                                ; 88 da                       ; 0xc1efd
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1eff
    xor dh, dh                                ; 30 f6                       ; 0xc1f02
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f04
    call 017fch                               ; e8 f2 f8                    ; 0xc1f07
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1f0a vgabios.c:1315
    jmp short 01eaeh                          ; eb 9f                       ; 0xc1f0d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f0f vgabios.c:1318
    xor ah, ah                                ; 30 e4                       ; 0xc1f12
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1f14
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f17
    xor ah, ah                                ; 30 e4                       ; 0xc1f1a
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f1c
    jnbe short 01f89h                         ; 77 68                       ; 0xc1f1f
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1f21 vgabios.c:1320
    xor dh, dh                                ; 30 f6                       ; 0xc1f24
    add ax, dx                                ; 01 d0                       ; 0xc1f26
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f28
    jnbe short 01f31h                         ; 77 04                       ; 0xc1f2b
    test dl, dl                               ; 84 d2                       ; 0xc1f2d
    jne short 01f5bh                          ; 75 2a                       ; 0xc1f2f
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f31 vgabios.c:1321
    xor ah, ah                                ; 30 e4                       ; 0xc1f34
    push ax                                   ; 50                          ; 0xc1f36
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f37
    push ax                                   ; 50                          ; 0xc1f3a
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1f3b
    xor ch, ch                                ; 30 ed                       ; 0xc1f3e
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1f40
    xor bh, bh                                ; 30 ff                       ; 0xc1f43
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1f45
    xor dh, dh                                ; 30 f6                       ; 0xc1f48
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f4a
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1f4d
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc1f50
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1f53
    call 018b2h                               ; e8 59 f9                    ; 0xc1f56
    jmp short 01f7ah                          ; eb 1f                       ; 0xc1f59 vgabios.c:1322
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f5b vgabios.c:1323
    xor ah, ah                                ; 30 e4                       ; 0xc1f5e
    push ax                                   ; 50                          ; 0xc1f60
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1f61
    push ax                                   ; 50                          ; 0xc1f64
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f65
    xor ch, ch                                ; 30 ed                       ; 0xc1f68
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1f6a
    xor bh, bh                                ; 30 ff                       ; 0xc1f6d
    mov dl, bl                                ; 88 da                       ; 0xc1f6f
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1f71
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f74
    call 017fch                               ; e8 82 f8                    ; 0xc1f77
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f7a vgabios.c:1324
    xor ah, ah                                ; 30 e4                       ; 0xc1f7d
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f7f
    jc short 01fc9h                           ; 72 45                       ; 0xc1f82
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1f84 vgabios.c:1325
    jmp short 01f17h                          ; eb 8e                       ; 0xc1f87
    jmp short 01fc9h                          ; eb 3e                       ; 0xc1f89
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f8b vgabios.c:1330
    jne short 01fcch                          ; 75 3b                       ; 0xc1f8f
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1f91
    jne short 01fcch                          ; 75 35                       ; 0xc1f95
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f97
    jne short 01fcch                          ; 75 2f                       ; 0xc1f9b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f9d
    cmp ax, di                                ; 39 f8                       ; 0xc1fa0
    jne short 01fcch                          ; 75 28                       ; 0xc1fa2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1fa4
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1fa7
    jne short 01fcch                          ; 75 20                       ; 0xc1faa
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1fac vgabios.c:1332
    xor dh, dh                                ; 30 f6                       ; 0xc1faf
    mov ax, cx                                ; 89 c8                       ; 0xc1fb1
    mul dx                                    ; f7 e2                       ; 0xc1fb3
    mov cx, ax                                ; 89 c1                       ; 0xc1fb5
    sal cx, 003h                              ; c1 e1 03                    ; 0xc1fb7
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1fba
    xor ah, ah                                ; 30 e4                       ; 0xc1fbd
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1fbf
    xor di, di                                ; 31 ff                       ; 0xc1fc3
    jcxz 01fc9h                               ; e3 02                       ; 0xc1fc5
    rep stosb                                 ; f3 aa                       ; 0xc1fc7
    jmp near 020b3h                           ; e9 e7 00                    ; 0xc1fc9 vgabios.c:1334
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1fcc vgabios.c:1337
    jne short 02041h                          ; 75 6f                       ; 0xc1fd0
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1fd2 vgabios.c:1338
    xor ah, ah                                ; 30 e4                       ; 0xc1fd5
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1fd7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1fda
    xor ah, ah                                ; 30 e4                       ; 0xc1fdd
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1fdf
    jc short 01fc9h                           ; 72 e5                       ; 0xc1fe2
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1fe4 vgabios.c:1340
    xor dh, dh                                ; 30 f6                       ; 0xc1fe7
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1fe9
    cmp dx, ax                                ; 39 c2                       ; 0xc1fec
    jnbe short 01ff6h                         ; 77 06                       ; 0xc1fee
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ff0
    jne short 02015h                          ; 75 1f                       ; 0xc1ff4
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1ff6 vgabios.c:1341
    xor ah, ah                                ; 30 e4                       ; 0xc1ff9
    push ax                                   ; 50                          ; 0xc1ffb
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1ffc
    push ax                                   ; 50                          ; 0xc1fff
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2000
    xor bh, bh                                ; 30 ff                       ; 0xc2003
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc2005
    xor dh, dh                                ; 30 f6                       ; 0xc2008
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc200a
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc200d
    call 019c7h                               ; e8 b4 f9                    ; 0xc2010
    jmp short 0203ch                          ; eb 27                       ; 0xc2013 vgabios.c:1342
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2015 vgabios.c:1343
    push ax                                   ; 50                          ; 0xc2018
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc2019
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc201c
    xor ch, ch                                ; 30 ed                       ; 0xc201f
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2021
    xor bh, bh                                ; 30 ff                       ; 0xc2024
    mov dl, bl                                ; 88 da                       ; 0xc2026
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc2028
    xor dh, dh                                ; 30 f6                       ; 0xc202b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc202d
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc2030
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc2033
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc2036
    call 01946h                               ; e8 0a f9                    ; 0xc2039
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc203c vgabios.c:1344
    jmp short 01fdah                          ; eb 99                       ; 0xc203f
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2041 vgabios.c:1347
    xor ah, ah                                ; 30 e4                       ; 0xc2044
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc2046
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2049
    xor ah, ah                                ; 30 e4                       ; 0xc204c
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc204e
    jnbe short 020b3h                         ; 77 60                       ; 0xc2051
    mov dl, al                                ; 88 c2                       ; 0xc2053 vgabios.c:1349
    xor dh, dh                                ; 30 f6                       ; 0xc2055
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2057
    add ax, dx                                ; 01 d0                       ; 0xc205a
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc205c
    jnbe short 02067h                         ; 77 06                       ; 0xc205f
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2061
    jne short 02086h                          ; 75 1f                       ; 0xc2065
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2067 vgabios.c:1350
    xor ah, ah                                ; 30 e4                       ; 0xc206a
    push ax                                   ; 50                          ; 0xc206c
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc206d
    push ax                                   ; 50                          ; 0xc2070
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2071
    xor bh, bh                                ; 30 ff                       ; 0xc2074
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc2076
    xor dh, dh                                ; 30 f6                       ; 0xc2079
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc207b
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc207e
    call 019c7h                               ; e8 43 f9                    ; 0xc2081
    jmp short 020a4h                          ; eb 1e                       ; 0xc2084 vgabios.c:1351
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2086 vgabios.c:1352
    xor ah, ah                                ; 30 e4                       ; 0xc2089
    push ax                                   ; 50                          ; 0xc208b
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc208c
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc208f
    xor ch, ch                                ; 30 ed                       ; 0xc2092
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2094
    xor bh, bh                                ; 30 ff                       ; 0xc2097
    mov dl, bl                                ; 88 da                       ; 0xc2099
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc209b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc209e
    call 01946h                               ; e8 a2 f8                    ; 0xc20a1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20a4 vgabios.c:1353
    xor ah, ah                                ; 30 e4                       ; 0xc20a7
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc20a9
    jc short 020b3h                           ; 72 05                       ; 0xc20ac
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc20ae vgabios.c:1354
    jmp short 02049h                          ; eb 96                       ; 0xc20b1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc20b3 vgabios.c:1365
    pop di                                    ; 5f                          ; 0xc20b6
    pop si                                    ; 5e                          ; 0xc20b7
    pop bp                                    ; 5d                          ; 0xc20b8
    retn 00008h                               ; c2 08 00                    ; 0xc20b9
  ; disGetNextSymbol 0xc20bc LB 0x21b3 -> off=0x0 cb=0000000000000111 uValue=00000000000c20bc 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc20bc LB 0x111
    push bp                                   ; 55                          ; 0xc20bc vgabios.c:1368
    mov bp, sp                                ; 89 e5                       ; 0xc20bd
    push si                                   ; 56                          ; 0xc20bf
    push di                                   ; 57                          ; 0xc20c0
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc20c1
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc20c4
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc20c7
    mov ch, bl                                ; 88 dd                       ; 0xc20ca
    mov al, cl                                ; 88 c8                       ; 0xc20cc
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc20ce vgabios.c:57
    xor dx, dx                                ; 31 d2                       ; 0xc20d1
    mov es, dx                                ; 8e c2                       ; 0xc20d3
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc20d5
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc20d8
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc20dc vgabios.c:58
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc20df
    xor ah, ah                                ; 30 e4                       ; 0xc20e2 vgabios.c:1377
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc20e4
    xor bh, bh                                ; 30 ff                       ; 0xc20e7
    imul bx                                   ; f7 eb                       ; 0xc20e9
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc20eb
    xor dh, dh                                ; 30 f6                       ; 0xc20ee
    imul dx                                   ; f7 ea                       ; 0xc20f0
    mov si, ax                                ; 89 c6                       ; 0xc20f2
    mov al, ch                                ; 88 e8                       ; 0xc20f4
    xor ah, ah                                ; 30 e4                       ; 0xc20f6
    add si, ax                                ; 01 c6                       ; 0xc20f8
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc20fa vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc20fd
    mov es, ax                                ; 8e c0                       ; 0xc2100
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc2102
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc2105 vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc2108
    mul dx                                    ; f7 e2                       ; 0xc210a
    add si, ax                                ; 01 c6                       ; 0xc210c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc210e vgabios.c:1379
    xor ah, ah                                ; 30 e4                       ; 0xc2111
    imul bx                                   ; f7 eb                       ; 0xc2113
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc2115
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc2118 vgabios.c:1380
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc211b
    out DX, ax                                ; ef                          ; 0xc211e
    mov ax, 00205h                            ; b8 05 02                    ; 0xc211f vgabios.c:1381
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2122
    out DX, ax                                ; ef                          ; 0xc2125
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc2126 vgabios.c:1382
    je short 02132h                           ; 74 06                       ; 0xc212a
    mov ax, 01803h                            ; b8 03 18                    ; 0xc212c vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc212f
    jmp short 02136h                          ; eb 04                       ; 0xc2130 vgabios.c:1386
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2132 vgabios.c:1388
    out DX, ax                                ; ef                          ; 0xc2135
    xor ch, ch                                ; 30 ed                       ; 0xc2136 vgabios.c:1390
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc2138
    jnc short 021afh                          ; 73 72                       ; 0xc213b
    mov al, ch                                ; 88 e8                       ; 0xc213d vgabios.c:1392
    xor ah, ah                                ; 30 e4                       ; 0xc213f
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2141
    xor bh, bh                                ; 30 ff                       ; 0xc2144
    imul bx                                   ; f7 eb                       ; 0xc2146
    mov bx, si                                ; 89 f3                       ; 0xc2148
    add bx, ax                                ; 01 c3                       ; 0xc214a
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc214c vgabios.c:1393
    jmp short 02164h                          ; eb 12                       ; 0xc2150
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2152 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc2155
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2157
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc215b vgabios.c:1406
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc215e
    jnc short 021b1h                          ; 73 4d                       ; 0xc2162
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2164
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2167
    sar ax, CL                                ; d3 f8                       ; 0xc216a
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc216c
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc216f
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2173
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2176
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2179
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc217b
    out DX, ax                                ; ef                          ; 0xc217e
    mov dx, bx                                ; 89 da                       ; 0xc217f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2181
    call 035dbh                               ; e8 54 14                    ; 0xc2184
    mov al, ch                                ; 88 e8                       ; 0xc2187
    xor ah, ah                                ; 30 e4                       ; 0xc2189
    add ax, word [bp-010h]                    ; 03 46 f0                    ; 0xc218b
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc218e
    mov di, word [bp-012h]                    ; 8b 7e ee                    ; 0xc2191
    add di, ax                                ; 01 c7                       ; 0xc2194
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc2196
    xor ah, ah                                ; 30 e4                       ; 0xc2199
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc219b
    je short 02152h                           ; 74 b2                       ; 0xc219e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc21a0
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc21a3
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc21a5
    mov es, dx                                ; 8e c2                       ; 0xc21a8
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc21aa
    jmp short 0215bh                          ; eb ac                       ; 0xc21ad
    jmp short 021b5h                          ; eb 04                       ; 0xc21af
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc21b1 vgabios.c:1407
    jmp short 02138h                          ; eb 83                       ; 0xc21b3
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc21b5 vgabios.c:1408
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21b8
    out DX, ax                                ; ef                          ; 0xc21bb
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc21bc vgabios.c:1409
    out DX, ax                                ; ef                          ; 0xc21bf
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc21c0 vgabios.c:1410
    out DX, ax                                ; ef                          ; 0xc21c3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc21c4 vgabios.c:1411
    pop di                                    ; 5f                          ; 0xc21c7
    pop si                                    ; 5e                          ; 0xc21c8
    pop bp                                    ; 5d                          ; 0xc21c9
    retn 00006h                               ; c2 06 00                    ; 0xc21ca
  ; disGetNextSymbol 0xc21cd LB 0x20a2 -> off=0x0 cb=0000000000000112 uValue=00000000000c21cd 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc21cd LB 0x112
    push si                                   ; 56                          ; 0xc21cd vgabios.c:1414
    push di                                   ; 57                          ; 0xc21ce
    enter 0000ch, 000h                        ; c8 0c 00 00                 ; 0xc21cf
    mov bh, al                                ; 88 c7                       ; 0xc21d3
    mov ch, dl                                ; 88 d5                       ; 0xc21d5
    mov al, bl                                ; 88 d8                       ; 0xc21d7
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc21d9 vgabios.c:1421
    xor ah, ah                                ; 30 e4                       ; 0xc21dc vgabios.c:1422
    mov dl, byte [bp+00ah]                    ; 8a 56 0a                    ; 0xc21de
    xor dh, dh                                ; 30 f6                       ; 0xc21e1
    imul dx                                   ; f7 ea                       ; 0xc21e3
    mov dl, cl                                ; 88 ca                       ; 0xc21e5
    xor dh, dh                                ; 30 f6                       ; 0xc21e7
    imul dx, dx, 00140h                       ; 69 d2 40 01                 ; 0xc21e9
    add ax, dx                                ; 01 d0                       ; 0xc21ed
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc21ef
    mov al, bh                                ; 88 f8                       ; 0xc21f2 vgabios.c:1423
    xor ah, ah                                ; 30 e4                       ; 0xc21f4
    sal ax, 003h                              ; c1 e0 03                    ; 0xc21f6
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc21f9
    xor ah, ah                                ; 30 e4                       ; 0xc21fc vgabios.c:1424
    jmp near 0221dh                           ; e9 1c 00                    ; 0xc21fe
    mov dl, ah                                ; 88 e2                       ; 0xc2201 vgabios.c:1439
    xor dh, dh                                ; 30 f6                       ; 0xc2203
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc2205
    mov si, di                                ; 89 fe                       ; 0xc2208
    add si, dx                                ; 01 d6                       ; 0xc220a
    mov al, byte [si]                         ; 8a 04                       ; 0xc220c
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc220e vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc2211
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2213
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2216 vgabios.c:1443
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc2218
    jnc short 02274h                          ; 73 57                       ; 0xc221b
    mov dl, ah                                ; 88 e2                       ; 0xc221d
    xor dh, dh                                ; 30 f6                       ; 0xc221f
    sar dx, 1                                 ; d1 fa                       ; 0xc2221
    imul dx, dx, strict byte 00050h           ; 6b d2 50                    ; 0xc2223
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2226
    add bx, dx                                ; 01 d3                       ; 0xc2229
    test ah, 001h                             ; f6 c4 01                    ; 0xc222b
    je short 02233h                           ; 74 03                       ; 0xc222e
    add bh, 020h                              ; 80 c7 20                    ; 0xc2230
    mov byte [bp-002h], 080h                  ; c6 46 fe 80                 ; 0xc2233
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2237
    jne short 02259h                          ; 75 1c                       ; 0xc223b
    test ch, 080h                             ; f6 c5 80                    ; 0xc223d
    je short 02201h                           ; 74 bf                       ; 0xc2240
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2242
    mov es, dx                                ; 8e c2                       ; 0xc2245
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2247
    mov dl, ah                                ; 88 e2                       ; 0xc224a
    xor dh, dh                                ; 30 f6                       ; 0xc224c
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc224e
    mov si, di                                ; 89 fe                       ; 0xc2251
    add si, dx                                ; 01 d6                       ; 0xc2253
    xor al, byte [si]                         ; 32 04                       ; 0xc2255
    jmp short 0220eh                          ; eb b5                       ; 0xc2257
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00                 ; 0xc2259 vgabios.c:1445
    jbe short 02216h                          ; 76 b7                       ; 0xc225d
    test ch, 080h                             ; f6 c5 80                    ; 0xc225f vgabios.c:1447
    je short 0226eh                           ; 74 0a                       ; 0xc2262
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2264 vgabios.c:37
    mov es, dx                                ; 8e c2                       ; 0xc2267
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2269
    jmp short 02270h                          ; eb 02                       ; 0xc226c vgabios.c:1451
    xor al, al                                ; 30 c0                       ; 0xc226e vgabios.c:1453
    xor dl, dl                                ; 30 d2                       ; 0xc2270 vgabios.c:1455
    jmp short 0227bh                          ; eb 07                       ; 0xc2272
    jmp short 022d9h                          ; eb 63                       ; 0xc2274
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc2276
    jnc short 022ceh                          ; 73 53                       ; 0xc2279
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc227b vgabios.c:1457
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc227e
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc2282
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2285
    add si, di                                ; 01 fe                       ; 0xc2288
    mov dh, byte [si]                         ; 8a 34                       ; 0xc228a
    mov byte [bp-006h], dh                    ; 88 76 fa                    ; 0xc228c
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc228f
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc2293
    mov byte [bp-00ah], dh                    ; 88 76 f6                    ; 0xc2296
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc2299
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc229d
    test word [bp-00ah], si                   ; 85 76 f6                    ; 0xc22a0
    je short 022c7h                           ; 74 22                       ; 0xc22a3
    mov DH, strict byte 003h                  ; b6 03                       ; 0xc22a5 vgabios.c:1458
    sub dh, dl                                ; 28 d6                       ; 0xc22a7
    mov cl, ch                                ; 88 e9                       ; 0xc22a9
    and cl, 003h                              ; 80 e1 03                    ; 0xc22ab
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc22ae
    mov cl, dh                                ; 88 f1                       ; 0xc22b1
    add cl, dh                                ; 00 f1                       ; 0xc22b3
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc22b5
    sal dh, CL                                ; d2 e6                       ; 0xc22b8
    mov cl, dh                                ; 88 f1                       ; 0xc22ba
    test ch, 080h                             ; f6 c5 80                    ; 0xc22bc vgabios.c:1459
    je short 022c5h                           ; 74 04                       ; 0xc22bf
    xor al, dh                                ; 30 f0                       ; 0xc22c1 vgabios.c:1461
    jmp short 022c7h                          ; eb 02                       ; 0xc22c3 vgabios.c:1463
    or al, dh                                 ; 08 f0                       ; 0xc22c5 vgabios.c:1465
    shr byte [bp-002h], 1                     ; d0 6e fe                    ; 0xc22c7 vgabios.c:1468
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2                     ; 0xc22ca vgabios.c:1469
    jmp short 02276h                          ; eb a8                       ; 0xc22cc
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc22ce vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc22d1
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc22d3
    inc bx                                    ; 43                          ; 0xc22d6 vgabios.c:1471
    jmp short 02259h                          ; eb 80                       ; 0xc22d7 vgabios.c:1472
    leave                                     ; c9                          ; 0xc22d9 vgabios.c:1475
    pop di                                    ; 5f                          ; 0xc22da
    pop si                                    ; 5e                          ; 0xc22db
    retn 00004h                               ; c2 04 00                    ; 0xc22dc
  ; disGetNextSymbol 0xc22df LB 0x1f90 -> off=0x0 cb=000000000000009b uValue=00000000000c22df 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc22df LB 0x9b
    push si                                   ; 56                          ; 0xc22df vgabios.c:1478
    push di                                   ; 57                          ; 0xc22e0
    enter 00008h, 000h                        ; c8 08 00 00                 ; 0xc22e1
    mov bh, al                                ; 88 c7                       ; 0xc22e5
    mov ch, dl                                ; 88 d5                       ; 0xc22e7
    mov al, cl                                ; 88 c8                       ; 0xc22e9
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc22eb vgabios.c:1485
    xor ah, ah                                ; 30 e4                       ; 0xc22ee vgabios.c:1486
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc22f0
    xor dh, dh                                ; 30 f6                       ; 0xc22f3
    imul dx                                   ; f7 ea                       ; 0xc22f5
    mov dx, ax                                ; 89 c2                       ; 0xc22f7
    sal dx, 006h                              ; c1 e2 06                    ; 0xc22f9
    mov al, bl                                ; 88 d8                       ; 0xc22fc
    xor ah, ah                                ; 30 e4                       ; 0xc22fe
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2300
    add ax, dx                                ; 01 d0                       ; 0xc2303
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc2305
    mov al, bh                                ; 88 f8                       ; 0xc2308 vgabios.c:1487
    xor ah, ah                                ; 30 e4                       ; 0xc230a
    sal ax, 003h                              ; c1 e0 03                    ; 0xc230c
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc230f
    xor bl, bl                                ; 30 db                       ; 0xc2312 vgabios.c:1488
    jmp short 02358h                          ; eb 42                       ; 0xc2314
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc2316 vgabios.c:1492
    jnc short 02351h                          ; 73 37                       ; 0xc2318
    xor bh, bh                                ; 30 ff                       ; 0xc231a vgabios.c:1494
    mov dl, bl                                ; 88 da                       ; 0xc231c vgabios.c:1495
    xor dh, dh                                ; 30 f6                       ; 0xc231e
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc2320
    mov si, di                                ; 89 fe                       ; 0xc2323
    add si, dx                                ; 01 d6                       ; 0xc2325
    mov dl, byte [si]                         ; 8a 14                       ; 0xc2327
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc2329
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc232c
    mov dl, ah                                ; 88 e2                       ; 0xc232f
    xor dh, dh                                ; 30 f6                       ; 0xc2331
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc2333
    je short 0233ah                           ; 74 02                       ; 0xc2336
    mov bh, ch                                ; 88 ef                       ; 0xc2338 vgabios.c:1497
    mov dl, al                                ; 88 c2                       ; 0xc233a vgabios.c:1499
    xor dh, dh                                ; 30 f6                       ; 0xc233c
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc233e
    add si, dx                                ; 01 d6                       ; 0xc2341
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc2343 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc2346
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc2348
    shr ah, 1                                 ; d0 ec                       ; 0xc234b vgabios.c:1500
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc234d vgabios.c:1501
    jmp short 02316h                          ; eb c5                       ; 0xc234f
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc2351 vgabios.c:1502
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2353
    jnc short 02374h                          ; 73 1c                       ; 0xc2356
    mov al, bl                                ; 88 d8                       ; 0xc2358
    xor ah, ah                                ; 30 e4                       ; 0xc235a
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc235c
    xor dh, dh                                ; 30 f6                       ; 0xc235f
    imul dx                                   ; f7 ea                       ; 0xc2361
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2363
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc2366
    add dx, ax                                ; 01 c2                       ; 0xc2369
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc236b
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc236e
    xor al, al                                ; 30 c0                       ; 0xc2370
    jmp short 0231ah                          ; eb a6                       ; 0xc2372
    leave                                     ; c9                          ; 0xc2374 vgabios.c:1503
    pop di                                    ; 5f                          ; 0xc2375
    pop si                                    ; 5e                          ; 0xc2376
    retn 00002h                               ; c2 02 00                    ; 0xc2377
  ; disGetNextSymbol 0xc237a LB 0x1ef5 -> off=0x0 cb=0000000000000187 uValue=00000000000c237a 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc237a LB 0x187
    push bp                                   ; 55                          ; 0xc237a vgabios.c:1506
    mov bp, sp                                ; 89 e5                       ; 0xc237b
    push si                                   ; 56                          ; 0xc237d
    push di                                   ; 57                          ; 0xc237e
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc237f
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2382
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2385
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2388
    mov si, cx                                ; 89 ce                       ; 0xc238b
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc238d vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2390
    mov es, ax                                ; 8e c0                       ; 0xc2393
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2395
    xor ah, ah                                ; 30 e4                       ; 0xc2398 vgabios.c:1514
    call 035b3h                               ; e8 16 12                    ; 0xc239a
    mov cl, al                                ; 88 c1                       ; 0xc239d
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc239f
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc23a2 vgabios.c:1515
    jne short 023a9h                          ; 75 03                       ; 0xc23a4
    jmp near 024fah                           ; e9 51 01                    ; 0xc23a6
    mov al, dl                                ; 88 d0                       ; 0xc23a9 vgabios.c:1518
    xor ah, ah                                ; 30 e4                       ; 0xc23ab
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc23ad
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc23b0
    call 00a1ah                               ; e8 64 e6                    ; 0xc23b3
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc23b6 vgabios.c:1519
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc23b9
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc23bc
    xor al, al                                ; 30 c0                       ; 0xc23bf
    shr ax, 008h                              ; c1 e8 08                    ; 0xc23c1
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc23c4
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc23c7
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc23ca
    mov bx, 00084h                            ; bb 84 00                    ; 0xc23cd vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23d0
    mov es, ax                                ; 8e c0                       ; 0xc23d3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc23d5
    xor ah, ah                                ; 30 e4                       ; 0xc23d8 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc23da
    inc dx                                    ; 42                          ; 0xc23dc
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc23dd vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc23e0
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc23e3
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc23e6 vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc23e9 vgabios.c:1525
    xor bh, bh                                ; 30 ff                       ; 0xc23eb
    mov di, bx                                ; 89 df                       ; 0xc23ed
    sal di, 003h                              ; c1 e7 03                    ; 0xc23ef
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc23f2
    jne short 02442h                          ; 75 49                       ; 0xc23f7
    mul dx                                    ; f7 e2                       ; 0xc23f9 vgabios.c:1528
    add ax, ax                                ; 01 c0                       ; 0xc23fb
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc23fd
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc23ff
    xor dh, dh                                ; 30 f6                       ; 0xc2402
    inc ax                                    ; 40                          ; 0xc2404
    mul dx                                    ; f7 e2                       ; 0xc2405
    mov bx, ax                                ; 89 c3                       ; 0xc2407
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2409
    xor ah, ah                                ; 30 e4                       ; 0xc240c
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc240e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2411
    xor dh, dh                                ; 30 f6                       ; 0xc2414
    add ax, dx                                ; 01 d0                       ; 0xc2416
    add ax, ax                                ; 01 c0                       ; 0xc2418
    mov dx, bx                                ; 89 da                       ; 0xc241a
    add dx, ax                                ; 01 c2                       ; 0xc241c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc241e vgabios.c:1530
    xor ah, ah                                ; 30 e4                       ; 0xc2421
    mov bx, ax                                ; 89 c3                       ; 0xc2423
    sal bx, 008h                              ; c1 e3 08                    ; 0xc2425
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2428
    add bx, ax                                ; 01 c3                       ; 0xc242b
    mov word [bp-020h], bx                    ; 89 5e e0                    ; 0xc242d
    mov ax, word [bp-020h]                    ; 8b 46 e0                    ; 0xc2430 vgabios.c:1531
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2433
    mov cx, si                                ; 89 f1                       ; 0xc2437
    mov di, dx                                ; 89 d7                       ; 0xc2439
    jcxz 0243fh                               ; e3 02                       ; 0xc243b
    rep stosw                                 ; f3 ab                       ; 0xc243d
    jmp near 024fah                           ; e9 b8 00                    ; 0xc243f vgabios.c:1533
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc2442 vgabios.c:1536
    sal bx, 006h                              ; c1 e3 06                    ; 0xc2446
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc2449
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc244d
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc2450 vgabios.c:1537
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2454
    dec si                                    ; 4e                          ; 0xc2457 vgabios.c:1538
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2458
    je short 024adh                           ; 74 50                       ; 0xc245b
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc245d vgabios.c:1540
    xor bh, bh                                ; 30 ff                       ; 0xc2460
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2462
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2465
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2469
    jc short 0247dh                           ; 72 0f                       ; 0xc246c
    jbe short 02484h                          ; 76 14                       ; 0xc246e
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2470
    je short 024d9h                           ; 74 64                       ; 0xc2473
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2475
    je short 02488h                           ; 74 0e                       ; 0xc2478
    jmp near 024f4h                           ; e9 77 00                    ; 0xc247a
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc247d
    je short 024afh                           ; 74 2d                       ; 0xc2480
    jmp short 024f4h                          ; eb 70                       ; 0xc2482
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc2484 vgabios.c:1543
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2488 vgabios.c:1545
    xor ah, ah                                ; 30 e4                       ; 0xc248b
    push ax                                   ; 50                          ; 0xc248d
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc248e
    push ax                                   ; 50                          ; 0xc2491
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2492
    push ax                                   ; 50                          ; 0xc2495
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2496
    xor ch, ch                                ; 30 ed                       ; 0xc2499
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc249b
    xor bh, bh                                ; 30 ff                       ; 0xc249e
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc24a0
    xor dh, dh                                ; 30 f6                       ; 0xc24a3
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc24a5
    call 020bch                               ; e8 11 fc                    ; 0xc24a8
    jmp short 024f4h                          ; eb 47                       ; 0xc24ab vgabios.c:1546
    jmp short 024fah                          ; eb 4b                       ; 0xc24ad
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc24af vgabios.c:1548
    xor ah, ah                                ; 30 e4                       ; 0xc24b2
    push ax                                   ; 50                          ; 0xc24b4
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24b5
    push ax                                   ; 50                          ; 0xc24b8
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc24b9
    xor ch, ch                                ; 30 ed                       ; 0xc24bc
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24be
    xor bh, bh                                ; 30 ff                       ; 0xc24c1
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc24c3
    xor dh, dh                                ; 30 f6                       ; 0xc24c6
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc24c8
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc24cb
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc24ce
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc24d1
    call 021cdh                               ; e8 f6 fc                    ; 0xc24d4
    jmp short 024f4h                          ; eb 1b                       ; 0xc24d7 vgabios.c:1549
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24d9 vgabios.c:1551
    xor ah, ah                                ; 30 e4                       ; 0xc24dc
    push ax                                   ; 50                          ; 0xc24de
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc24df
    xor ch, ch                                ; 30 ed                       ; 0xc24e2
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24e4
    xor bh, bh                                ; 30 ff                       ; 0xc24e7
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc24e9
    xor dh, dh                                ; 30 f6                       ; 0xc24ec
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc24ee
    call 022dfh                               ; e8 eb fd                    ; 0xc24f1
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc24f4 vgabios.c:1558
    jmp near 02457h                           ; e9 5d ff                    ; 0xc24f7 vgabios.c:1559
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc24fa vgabios.c:1561
    pop di                                    ; 5f                          ; 0xc24fd
    pop si                                    ; 5e                          ; 0xc24fe
    pop bp                                    ; 5d                          ; 0xc24ff
    retn                                      ; c3                          ; 0xc2500
  ; disGetNextSymbol 0xc2501 LB 0x1d6e -> off=0x0 cb=0000000000000181 uValue=00000000000c2501 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2501 LB 0x181
    push bp                                   ; 55                          ; 0xc2501 vgabios.c:1564
    mov bp, sp                                ; 89 e5                       ; 0xc2502
    push si                                   ; 56                          ; 0xc2504
    push di                                   ; 57                          ; 0xc2505
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc2506
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2509
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc250c
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc250f
    mov si, cx                                ; 89 ce                       ; 0xc2512
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2514 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2517
    mov es, ax                                ; 8e c0                       ; 0xc251a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc251c
    xor ah, ah                                ; 30 e4                       ; 0xc251f vgabios.c:1572
    call 035b3h                               ; e8 8f 10                    ; 0xc2521
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2524
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2527
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc252a vgabios.c:1573
    jne short 02531h                          ; 75 03                       ; 0xc252c
    jmp near 0267bh                           ; e9 4a 01                    ; 0xc252e
    mov al, dl                                ; 88 d0                       ; 0xc2531 vgabios.c:1576
    xor ah, ah                                ; 30 e4                       ; 0xc2533
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc2535
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc2538
    call 00a1ah                               ; e8 dc e4                    ; 0xc253b
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc253e vgabios.c:1577
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2541
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2544
    xor al, al                                ; 30 c0                       ; 0xc2547
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2549
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc254c
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc254f
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2552
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2555 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2558
    mov es, ax                                ; 8e c0                       ; 0xc255b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc255d
    xor ah, ah                                ; 30 e4                       ; 0xc2560 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc2562
    inc dx                                    ; 42                          ; 0xc2564
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2565 vgabios.c:47
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc2568
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc256b vgabios.c:48
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc256e vgabios.c:1583
    mov bx, ax                                ; 89 c3                       ; 0xc2571
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2573
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2576
    jne short 025bfh                          ; 75 42                       ; 0xc257b
    mov ax, cx                                ; 89 c8                       ; 0xc257d vgabios.c:1586
    mul dx                                    ; f7 e2                       ; 0xc257f
    add ax, ax                                ; 01 c0                       ; 0xc2581
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2583
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2585
    xor dh, dh                                ; 30 f6                       ; 0xc2588
    inc ax                                    ; 40                          ; 0xc258a
    mul dx                                    ; f7 e2                       ; 0xc258b
    mov bx, ax                                ; 89 c3                       ; 0xc258d
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc258f
    xor ah, ah                                ; 30 e4                       ; 0xc2592
    mul cx                                    ; f7 e1                       ; 0xc2594
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2596
    xor dh, dh                                ; 30 f6                       ; 0xc2599
    add ax, dx                                ; 01 d0                       ; 0xc259b
    add ax, ax                                ; 01 c0                       ; 0xc259d
    add bx, ax                                ; 01 c3                       ; 0xc259f
    dec si                                    ; 4e                          ; 0xc25a1 vgabios.c:1588
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25a2
    je short 0252eh                           ; 74 87                       ; 0xc25a5
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc25a7 vgabios.c:1589
    xor ah, ah                                ; 30 e4                       ; 0xc25aa
    mov di, ax                                ; 89 c7                       ; 0xc25ac
    sal di, 003h                              ; c1 e7 03                    ; 0xc25ae
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc25b1 vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc25b5 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc25b8
    inc bx                                    ; 43                          ; 0xc25bb vgabios.c:1590
    inc bx                                    ; 43                          ; 0xc25bc
    jmp short 025a1h                          ; eb e2                       ; 0xc25bd vgabios.c:1591
    mov di, ax                                ; 89 c7                       ; 0xc25bf vgabios.c:1596
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc25c1
    mov di, ax                                ; 89 c7                       ; 0xc25c5
    sal di, 006h                              ; c1 e7 06                    ; 0xc25c7
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc25ca
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc25ce
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc25d1 vgabios.c:1597
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc25d5
    dec si                                    ; 4e                          ; 0xc25d8 vgabios.c:1598
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25d9
    je short 0262eh                           ; 74 50                       ; 0xc25dc
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc25de vgabios.c:1600
    xor bh, bh                                ; 30 ff                       ; 0xc25e1
    sal bx, 003h                              ; c1 e3 03                    ; 0xc25e3
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc25e6
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc25ea
    jc short 025feh                           ; 72 0f                       ; 0xc25ed
    jbe short 02605h                          ; 76 14                       ; 0xc25ef
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc25f1
    je short 0265ah                           ; 74 64                       ; 0xc25f4
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc25f6
    je short 02609h                           ; 74 0e                       ; 0xc25f9
    jmp near 02675h                           ; e9 77 00                    ; 0xc25fb
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc25fe
    je short 02630h                           ; 74 2d                       ; 0xc2601
    jmp short 02675h                          ; eb 70                       ; 0xc2603
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2605 vgabios.c:1603
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2609 vgabios.c:1605
    xor ah, ah                                ; 30 e4                       ; 0xc260c
    push ax                                   ; 50                          ; 0xc260e
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc260f
    push ax                                   ; 50                          ; 0xc2612
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2613
    push ax                                   ; 50                          ; 0xc2616
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2617
    xor ch, ch                                ; 30 ed                       ; 0xc261a
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc261c
    xor bh, bh                                ; 30 ff                       ; 0xc261f
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2621
    xor dh, dh                                ; 30 f6                       ; 0xc2624
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2626
    call 020bch                               ; e8 90 fa                    ; 0xc2629
    jmp short 02675h                          ; eb 47                       ; 0xc262c vgabios.c:1606
    jmp short 0267bh                          ; eb 4b                       ; 0xc262e
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2630 vgabios.c:1608
    xor ah, ah                                ; 30 e4                       ; 0xc2633
    push ax                                   ; 50                          ; 0xc2635
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2636
    push ax                                   ; 50                          ; 0xc2639
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc263a
    xor ch, ch                                ; 30 ed                       ; 0xc263d
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc263f
    xor bh, bh                                ; 30 ff                       ; 0xc2642
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2644
    xor dh, dh                                ; 30 f6                       ; 0xc2647
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2649
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc264c
    mov byte [bp-019h], ah                    ; 88 66 e7                    ; 0xc264f
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2652
    call 021cdh                               ; e8 75 fb                    ; 0xc2655
    jmp short 02675h                          ; eb 1b                       ; 0xc2658 vgabios.c:1609
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc265a vgabios.c:1611
    xor ah, ah                                ; 30 e4                       ; 0xc265d
    push ax                                   ; 50                          ; 0xc265f
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2660
    xor ch, ch                                ; 30 ed                       ; 0xc2663
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2665
    xor bh, bh                                ; 30 ff                       ; 0xc2668
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc266a
    xor dh, dh                                ; 30 f6                       ; 0xc266d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc266f
    call 022dfh                               ; e8 6a fc                    ; 0xc2672
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2675 vgabios.c:1618
    jmp near 025d8h                           ; e9 5d ff                    ; 0xc2678 vgabios.c:1619
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc267b vgabios.c:1621
    pop di                                    ; 5f                          ; 0xc267e
    pop si                                    ; 5e                          ; 0xc267f
    pop bp                                    ; 5d                          ; 0xc2680
    retn                                      ; c3                          ; 0xc2681
  ; disGetNextSymbol 0xc2682 LB 0x1bed -> off=0x0 cb=0000000000000173 uValue=00000000000c2682 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc2682 LB 0x173
    push bp                                   ; 55                          ; 0xc2682 vgabios.c:1624
    mov bp, sp                                ; 89 e5                       ; 0xc2683
    push si                                   ; 56                          ; 0xc2685
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc2686
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2689
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc268c
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc268f
    mov dx, cx                                ; 89 ca                       ; 0xc2692
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2694 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2697
    mov es, ax                                ; 8e c0                       ; 0xc269a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc269c
    xor ah, ah                                ; 30 e4                       ; 0xc269f vgabios.c:1631
    call 035b3h                               ; e8 0f 0f                    ; 0xc26a1
    mov cl, al                                ; 88 c1                       ; 0xc26a4
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc26a6 vgabios.c:1632
    je short 026d0h                           ; 74 26                       ; 0xc26a8
    mov bl, al                                ; 88 c3                       ; 0xc26aa vgabios.c:1633
    xor bh, bh                                ; 30 ff                       ; 0xc26ac
    sal bx, 003h                              ; c1 e3 03                    ; 0xc26ae
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc26b1
    je short 026d0h                           ; 74 18                       ; 0xc26b6
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc26b8 vgabios.c:1635
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc26bc
    jc short 026cch                           ; 72 0c                       ; 0xc26be
    jbe short 026d6h                          ; 76 14                       ; 0xc26c0
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc26c2
    je short 026d3h                           ; 74 0d                       ; 0xc26c4
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc26c6
    je short 026d6h                           ; 74 0c                       ; 0xc26c8
    jmp short 026d0h                          ; eb 04                       ; 0xc26ca
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc26cc
    je short 02747h                           ; 74 77                       ; 0xc26ce
    jmp near 027efh                           ; e9 1c 01                    ; 0xc26d0
    jmp near 027cdh                           ; e9 f7 00                    ; 0xc26d3
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc26d6 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26d9
    mov es, ax                                ; 8e c0                       ; 0xc26dc
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc26de
    mov ax, dx                                ; 89 d0                       ; 0xc26e1 vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc26e3
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc26e5
    shr bx, 003h                              ; c1 eb 03                    ; 0xc26e8
    add bx, ax                                ; 01 c3                       ; 0xc26eb
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc26ed vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc26f0
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc26f3 vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc26f6
    mul dx                                    ; f7 e2                       ; 0xc26f8
    add bx, ax                                ; 01 c3                       ; 0xc26fa
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc26fc vgabios.c:1641
    and cl, 007h                              ; 80 e1 07                    ; 0xc26ff
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2702
    sar ax, CL                                ; d3 f8                       ; 0xc2705
    xor ah, ah                                ; 30 e4                       ; 0xc2707 vgabios.c:1642
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2709
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc270c
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc270e
    out DX, ax                                ; ef                          ; 0xc2711
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2712 vgabios.c:1643
    out DX, ax                                ; ef                          ; 0xc2715
    mov dx, bx                                ; 89 da                       ; 0xc2716 vgabios.c:1644
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2718
    call 035dbh                               ; e8 bd 0e                    ; 0xc271b
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc271e vgabios.c:1645
    je short 0272bh                           ; 74 07                       ; 0xc2722
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2724 vgabios.c:1647
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2727
    out DX, ax                                ; ef                          ; 0xc272a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc272b vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc272e
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2730
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2733
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2736 vgabios.c:1650
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2739
    out DX, ax                                ; ef                          ; 0xc273c
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc273d vgabios.c:1651
    out DX, ax                                ; ef                          ; 0xc2740
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2741 vgabios.c:1652
    out DX, ax                                ; ef                          ; 0xc2744
    jmp short 026d0h                          ; eb 89                       ; 0xc2745 vgabios.c:1653
    mov ax, dx                                ; 89 d0                       ; 0xc2747 vgabios.c:1655
    shr ax, 1                                 ; d1 e8                       ; 0xc2749
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc274b
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc274e
    jne short 0275dh                          ; 75 08                       ; 0xc2753
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2755 vgabios.c:1657
    shr bx, 002h                              ; c1 eb 02                    ; 0xc2758
    jmp short 02763h                          ; eb 06                       ; 0xc275b vgabios.c:1659
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc275d vgabios.c:1661
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2760
    add bx, ax                                ; 01 c3                       ; 0xc2763
    test dl, 001h                             ; f6 c2 01                    ; 0xc2765 vgabios.c:1663
    je short 0276dh                           ; 74 03                       ; 0xc2768
    add bh, 020h                              ; 80 c7 20                    ; 0xc276a
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc276d vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc2770
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2772
    mov al, cl                                ; 88 c8                       ; 0xc2775 vgabios.c:1665
    xor ah, ah                                ; 30 e4                       ; 0xc2777
    mov si, ax                                ; 89 c6                       ; 0xc2779
    sal si, 003h                              ; c1 e6 03                    ; 0xc277b
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc277e
    jne short 0279eh                          ; 75 19                       ; 0xc2783
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2785 vgabios.c:1667
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2788
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc278a
    sub ah, al                                ; 28 c4                       ; 0xc278c
    mov cl, ah                                ; 88 e1                       ; 0xc278e
    add cl, ah                                ; 00 e1                       ; 0xc2790
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc2792
    and dh, 003h                              ; 80 e6 03                    ; 0xc2795
    sal dh, CL                                ; d2 e6                       ; 0xc2798
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc279a vgabios.c:1668
    jmp short 027b1h                          ; eb 13                       ; 0xc279c vgabios.c:1670
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc279e vgabios.c:1672
    and AL, strict byte 007h                  ; 24 07                       ; 0xc27a1
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc27a3
    sub cl, al                                ; 28 c1                       ; 0xc27a5
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc27a7
    and dh, 001h                              ; 80 e6 01                    ; 0xc27aa
    sal dh, CL                                ; d2 e6                       ; 0xc27ad
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc27af vgabios.c:1673
    sal al, CL                                ; d2 e0                       ; 0xc27b1
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc27b3 vgabios.c:1675
    je short 027bdh                           ; 74 04                       ; 0xc27b7
    xor dl, dh                                ; 30 f2                       ; 0xc27b9 vgabios.c:1677
    jmp short 027c3h                          ; eb 06                       ; 0xc27bb vgabios.c:1679
    not al                                    ; f6 d0                       ; 0xc27bd vgabios.c:1681
    and dl, al                                ; 20 c2                       ; 0xc27bf
    or dl, dh                                 ; 08 f2                       ; 0xc27c1 vgabios.c:1682
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc27c3 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc27c6
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc27c8
    jmp short 027efh                          ; eb 22                       ; 0xc27cb vgabios.c:1685
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc27cd vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc27d0
    mov es, ax                                ; 8e c0                       ; 0xc27d3
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc27d5
    sal bx, 003h                              ; c1 e3 03                    ; 0xc27d8 vgabios.c:48
    mov ax, dx                                ; 89 d0                       ; 0xc27db
    mul bx                                    ; f7 e3                       ; 0xc27dd
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27df
    add bx, ax                                ; 01 c3                       ; 0xc27e2
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc27e4 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc27e7
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc27e9
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc27ec
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc27ef vgabios.c:1695
    pop si                                    ; 5e                          ; 0xc27f2
    pop bp                                    ; 5d                          ; 0xc27f3
    retn                                      ; c3                          ; 0xc27f4
  ; disGetNextSymbol 0xc27f5 LB 0x1a7a -> off=0x0 cb=0000000000000258 uValue=00000000000c27f5 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc27f5 LB 0x258
    push bp                                   ; 55                          ; 0xc27f5 vgabios.c:1698
    mov bp, sp                                ; 89 e5                       ; 0xc27f6
    push si                                   ; 56                          ; 0xc27f8
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc27f9
    mov ch, al                                ; 88 c5                       ; 0xc27fc
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc27fe
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2801
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2804 vgabios.c:1706
    jne short 02817h                          ; 75 0e                       ; 0xc2807
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc2809 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc280c
    mov es, ax                                ; 8e c0                       ; 0xc280f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2811
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2814 vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2817 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc281a
    mov es, ax                                ; 8e c0                       ; 0xc281d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc281f
    xor ah, ah                                ; 30 e4                       ; 0xc2822 vgabios.c:1711
    call 035b3h                               ; e8 8c 0d                    ; 0xc2824
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2827
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc282a vgabios.c:1712
    je short 02894h                           ; 74 66                       ; 0xc282c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc282e vgabios.c:1715
    xor ah, ah                                ; 30 e4                       ; 0xc2831
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc2833
    lea dx, [bp-016h]                         ; 8d 56 ea                    ; 0xc2836
    call 00a1ah                               ; e8 de e1                    ; 0xc2839
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc283c vgabios.c:1716
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc283f
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc2842
    xor al, al                                ; 30 c0                       ; 0xc2845
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2847
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc284a
    mov bx, 00084h                            ; bb 84 00                    ; 0xc284d vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2850
    mov es, dx                                ; 8e c2                       ; 0xc2853
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2855
    xor dh, dh                                ; 30 f6                       ; 0xc2858 vgabios.c:38
    inc dx                                    ; 42                          ; 0xc285a
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc285b
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc285e vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2861
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc2864 vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2867 vgabios.c:1722
    jc short 0287ah                           ; 72 0e                       ; 0xc286a
    jbe short 02882h                          ; 76 14                       ; 0xc286c
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc286e
    je short 02897h                           ; 74 24                       ; 0xc2871
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc2873
    je short 0288dh                           ; 74 15                       ; 0xc2876
    jmp short 0289eh                          ; eb 24                       ; 0xc2878
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc287a
    jne short 0289eh                          ; 75 1f                       ; 0xc287d
    jmp near 029a4h                           ; e9 22 01                    ; 0xc287f
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00                 ; 0xc2882 vgabios.c:1729
    jbe short 0289bh                          ; 76 13                       ; 0xc2886
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc2888
    jmp short 0289bh                          ; eb 0e                       ; 0xc288b vgabios.c:1730
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc288d vgabios.c:1733
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc288f
    jmp short 0289bh                          ; eb 07                       ; 0xc2892 vgabios.c:1734
    jmp near 02a47h                           ; e9 b0 01                    ; 0xc2894
    mov byte [bp-004h], 000h                  ; c6 46 fc 00                 ; 0xc2897 vgabios.c:1737
    jmp near 029a4h                           ; e9 06 01                    ; 0xc289b vgabios.c:1738
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc289e vgabios.c:1742
    xor ah, ah                                ; 30 e4                       ; 0xc28a1
    mov bx, ax                                ; 89 c3                       ; 0xc28a3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc28a5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc28a8
    jne short 028f1h                          ; 75 42                       ; 0xc28ad
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc28af vgabios.c:1745
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc28b2
    add ax, ax                                ; 01 c0                       ; 0xc28b5
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc28b7
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc28b9
    xor dh, dh                                ; 30 f6                       ; 0xc28bc
    inc ax                                    ; 40                          ; 0xc28be
    mul dx                                    ; f7 e2                       ; 0xc28bf
    mov si, ax                                ; 89 c6                       ; 0xc28c1
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc28c3
    xor ah, ah                                ; 30 e4                       ; 0xc28c6
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc28c8
    mov dx, ax                                ; 89 c2                       ; 0xc28cb
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc28cd
    xor ah, ah                                ; 30 e4                       ; 0xc28d0
    add ax, dx                                ; 01 d0                       ; 0xc28d2
    add ax, ax                                ; 01 c0                       ; 0xc28d4
    add si, ax                                ; 01 c6                       ; 0xc28d6
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc28d8 vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc28dc vgabios.c:42
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc28df vgabios.c:1750
    jne short 02920h                          ; 75 3c                       ; 0xc28e2
    inc si                                    ; 46                          ; 0xc28e4 vgabios.c:1751
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc28e5 vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc28e9
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc28ec
    jmp short 02920h                          ; eb 2f                       ; 0xc28ef vgabios.c:1753
    mov si, ax                                ; 89 c6                       ; 0xc28f1 vgabios.c:1756
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc28f3
    mov si, ax                                ; 89 c6                       ; 0xc28f7
    sal si, 006h                              ; c1 e6 06                    ; 0xc28f9
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc28fc
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2900 vgabios.c:1757
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2904 vgabios.c:1758
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2908
    jc short 0291bh                           ; 72 0e                       ; 0xc290b
    jbe short 02922h                          ; 76 13                       ; 0xc290d
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc290f
    je short 02972h                           ; 74 5e                       ; 0xc2912
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2914
    je short 02926h                           ; 74 0d                       ; 0xc2917
    jmp short 02991h                          ; eb 76                       ; 0xc2919
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc291b
    je short 02950h                           ; 74 30                       ; 0xc291e
    jmp short 02991h                          ; eb 6f                       ; 0xc2920
    or byte [bp-00ah], 001h                   ; 80 4e f6 01                 ; 0xc2922 vgabios.c:1761
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2926 vgabios.c:1763
    xor ah, ah                                ; 30 e4                       ; 0xc2929
    push ax                                   ; 50                          ; 0xc292b
    mov al, dl                                ; 88 d0                       ; 0xc292c
    push ax                                   ; 50                          ; 0xc292e
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc292f
    push ax                                   ; 50                          ; 0xc2932
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2933
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2936
    xor bh, bh                                ; 30 ff                       ; 0xc2939
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc293b
    xor dh, dh                                ; 30 f6                       ; 0xc293e
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc2940
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2943
    mov cx, ax                                ; 89 c1                       ; 0xc2946
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2948
    call 020bch                               ; e8 6e f7                    ; 0xc294b
    jmp short 02991h                          ; eb 41                       ; 0xc294e vgabios.c:1764
    push ax                                   ; 50                          ; 0xc2950 vgabios.c:1766
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2951
    push ax                                   ; 50                          ; 0xc2954
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2955
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2958
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc295b
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc295e
    xor bh, bh                                ; 30 ff                       ; 0xc2961
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2963
    xor dh, dh                                ; 30 f6                       ; 0xc2966
    mov al, ch                                ; 88 e8                       ; 0xc2968
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc296a
    call 021cdh                               ; e8 5d f8                    ; 0xc296d
    jmp short 02991h                          ; eb 1f                       ; 0xc2970 vgabios.c:1767
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2972 vgabios.c:1769
    push ax                                   ; 50                          ; 0xc2975
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2976
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2979
    xor bh, bh                                ; 30 ff                       ; 0xc297c
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc297e
    xor dh, dh                                ; 30 f6                       ; 0xc2981
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc2983
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2986
    mov cx, ax                                ; 89 c1                       ; 0xc2989
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc298b
    call 022dfh                               ; e8 4e f9                    ; 0xc298e
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc2991 vgabios.c:1777
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2994 vgabios.c:1779
    xor ah, ah                                ; 30 e4                       ; 0xc2997
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc2999
    jne short 029a4h                          ; 75 06                       ; 0xc299c
    mov byte [bp-004h], ah                    ; 88 66 fc                    ; 0xc299e vgabios.c:1780
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc29a1 vgabios.c:1781
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc29a4 vgabios.c:1786
    xor ah, ah                                ; 30 e4                       ; 0xc29a7
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc29a9
    jne short 02a0fh                          ; 75 61                       ; 0xc29ac
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc29ae vgabios.c:1788
    xor bh, bh                                ; 30 ff                       ; 0xc29b1
    sal bx, 003h                              ; c1 e3 03                    ; 0xc29b3
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc29b6
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc29b9
    mov cl, byte [bp-010h]                    ; 8a 4e f0                    ; 0xc29bb
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc29be
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc29c0
    jne short 02a11h                          ; 75 4a                       ; 0xc29c5
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29c7 vgabios.c:1790
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc29ca
    add ax, ax                                ; 01 c0                       ; 0xc29cd
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc29cf
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc29d1
    xor dh, dh                                ; 30 f6                       ; 0xc29d4
    inc ax                                    ; 40                          ; 0xc29d6
    mul dx                                    ; f7 e2                       ; 0xc29d7
    mov si, ax                                ; 89 c6                       ; 0xc29d9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc29db
    xor ah, ah                                ; 30 e4                       ; 0xc29de
    dec ax                                    ; 48                          ; 0xc29e0
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc29e1
    mov dx, ax                                ; 89 c2                       ; 0xc29e4
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc29e6
    xor ah, ah                                ; 30 e4                       ; 0xc29e9
    add ax, dx                                ; 01 d0                       ; 0xc29eb
    add ax, ax                                ; 01 c0                       ; 0xc29ed
    add si, ax                                ; 01 c6                       ; 0xc29ef
    inc si                                    ; 46                          ; 0xc29f1 vgabios.c:1791
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc29f2 vgabios.c:35
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc29f6
    push strict byte 00001h                   ; 6a 01                       ; 0xc29f9 vgabios.c:1792
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29fb
    xor ah, ah                                ; 30 e4                       ; 0xc29fe
    push ax                                   ; 50                          ; 0xc2a00
    mov al, cl                                ; 88 c8                       ; 0xc2a01
    push ax                                   ; 50                          ; 0xc2a03
    mov al, ch                                ; 88 e8                       ; 0xc2a04
    push ax                                   ; 50                          ; 0xc2a06
    xor dh, dh                                ; 30 f6                       ; 0xc2a07
    xor cx, cx                                ; 31 c9                       ; 0xc2a09
    xor bx, bx                                ; 31 db                       ; 0xc2a0b
    jmp short 02a23h                          ; eb 14                       ; 0xc2a0d vgabios.c:1794
    jmp short 02a2ch                          ; eb 1b                       ; 0xc2a0f
    push strict byte 00001h                   ; 6a 01                       ; 0xc2a11 vgabios.c:1796
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a13
    push ax                                   ; 50                          ; 0xc2a16
    mov al, cl                                ; 88 c8                       ; 0xc2a17
    push ax                                   ; 50                          ; 0xc2a19
    mov al, ch                                ; 88 e8                       ; 0xc2a1a
    push ax                                   ; 50                          ; 0xc2a1c
    xor cx, cx                                ; 31 c9                       ; 0xc2a1d
    xor bx, bx                                ; 31 db                       ; 0xc2a1f
    xor dx, dx                                ; 31 d2                       ; 0xc2a21
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a23
    call 01a34h                               ; e8 0b f0                    ; 0xc2a26
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2a29 vgabios.c:1798
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2a2c vgabios.c:1802
    xor ah, ah                                ; 30 e4                       ; 0xc2a2f
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc2a31
    sal word [bp-014h], 008h                  ; c1 66 ec 08                 ; 0xc2a34
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2a38
    add word [bp-014h], ax                    ; 01 46 ec                    ; 0xc2a3b
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc2a3e vgabios.c:1803
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a41
    call 01217h                               ; e8 d0 e7                    ; 0xc2a44
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a47 vgabios.c:1804
    pop si                                    ; 5e                          ; 0xc2a4a
    pop bp                                    ; 5d                          ; 0xc2a4b
    retn                                      ; c3                          ; 0xc2a4c
  ; disGetNextSymbol 0xc2a4d LB 0x1822 -> off=0x0 cb=000000000000002c uValue=00000000000c2a4d 'get_font_access'
get_font_access:                             ; 0xc2a4d LB 0x2c
    push bp                                   ; 55                          ; 0xc2a4d vgabios.c:1807
    mov bp, sp                                ; 89 e5                       ; 0xc2a4e
    push dx                                   ; 52                          ; 0xc2a50
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a51 vgabios.c:1809
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a54
    out DX, ax                                ; ef                          ; 0xc2a57
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2a58 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc2a5b
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2a5c vgabios.c:1811
    out DX, ax                                ; ef                          ; 0xc2a5f
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2a60 vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2a63
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2a64 vgabios.c:1813
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2a67
    out DX, ax                                ; ef                          ; 0xc2a6a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2a6b vgabios.c:1814
    out DX, ax                                ; ef                          ; 0xc2a6e
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2a6f vgabios.c:1815
    out DX, ax                                ; ef                          ; 0xc2a72
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a73 vgabios.c:1816
    pop dx                                    ; 5a                          ; 0xc2a76
    pop bp                                    ; 5d                          ; 0xc2a77
    retn                                      ; c3                          ; 0xc2a78
  ; disGetNextSymbol 0xc2a79 LB 0x17f6 -> off=0x0 cb=000000000000003c uValue=00000000000c2a79 'release_font_access'
release_font_access:                         ; 0xc2a79 LB 0x3c
    push bp                                   ; 55                          ; 0xc2a79 vgabios.c:1818
    mov bp, sp                                ; 89 e5                       ; 0xc2a7a
    push dx                                   ; 52                          ; 0xc2a7c
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a7d vgabios.c:1820
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a80
    out DX, ax                                ; ef                          ; 0xc2a83
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2a84 vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2a87
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2a88 vgabios.c:1822
    out DX, ax                                ; ef                          ; 0xc2a8b
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2a8c vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc2a8f
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2a90 vgabios.c:1824
    in AL, DX                                 ; ec                          ; 0xc2a93
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2a94
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2a96
    sal ax, 002h                              ; c1 e0 02                    ; 0xc2a99
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc2a9c
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2a9e
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2aa1
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2aa3
    out DX, ax                                ; ef                          ; 0xc2aa6
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2aa7 vgabios.c:1825
    out DX, ax                                ; ef                          ; 0xc2aaa
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2aab vgabios.c:1826
    out DX, ax                                ; ef                          ; 0xc2aae
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2aaf vgabios.c:1827
    pop dx                                    ; 5a                          ; 0xc2ab2
    pop bp                                    ; 5d                          ; 0xc2ab3
    retn                                      ; c3                          ; 0xc2ab4
  ; disGetNextSymbol 0xc2ab5 LB 0x17ba -> off=0x0 cb=00000000000000b1 uValue=00000000000c2ab5 'set_scan_lines'
set_scan_lines:                              ; 0xc2ab5 LB 0xb1
    push bp                                   ; 55                          ; 0xc2ab5 vgabios.c:1829
    mov bp, sp                                ; 89 e5                       ; 0xc2ab6
    push bx                                   ; 53                          ; 0xc2ab8
    push cx                                   ; 51                          ; 0xc2ab9
    push dx                                   ; 52                          ; 0xc2aba
    push si                                   ; 56                          ; 0xc2abb
    push di                                   ; 57                          ; 0xc2abc
    mov bl, al                                ; 88 c3                       ; 0xc2abd
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2abf vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ac2
    mov es, ax                                ; 8e c0                       ; 0xc2ac5
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2ac7
    mov cx, si                                ; 89 f1                       ; 0xc2aca vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2acc vgabios.c:1835
    mov dx, si                                ; 89 f2                       ; 0xc2ace
    out DX, AL                                ; ee                          ; 0xc2ad0
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2ad1 vgabios.c:1836
    in AL, DX                                 ; ec                          ; 0xc2ad4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ad5
    mov ah, al                                ; 88 c4                       ; 0xc2ad7 vgabios.c:1837
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2ad9
    mov al, bl                                ; 88 d8                       ; 0xc2adc
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2ade
    or al, ah                                 ; 08 e0                       ; 0xc2ae0
    out DX, AL                                ; ee                          ; 0xc2ae2 vgabios.c:1838
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2ae3 vgabios.c:1839
    jne short 02af0h                          ; 75 08                       ; 0xc2ae6
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2ae8 vgabios.c:1841
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2aeb
    jmp short 02afdh                          ; eb 0d                       ; 0xc2aee vgabios.c:1843
    mov dl, bl                                ; 88 da                       ; 0xc2af0 vgabios.c:1845
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2af2
    xor dh, dh                                ; 30 f6                       ; 0xc2af5
    mov al, bl                                ; 88 d8                       ; 0xc2af7
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2af9
    xor ah, ah                                ; 30 e4                       ; 0xc2afb
    call 01110h                               ; e8 10 e6                    ; 0xc2afd
    xor bh, bh                                ; 30 ff                       ; 0xc2b00 vgabios.c:1847
    mov si, 00085h                            ; be 85 00                    ; 0xc2b02 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b05
    mov es, ax                                ; 8e c0                       ; 0xc2b08
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2b0a
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2b0d vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2b0f
    out DX, AL                                ; ee                          ; 0xc2b11
    mov si, cx                                ; 89 ce                       ; 0xc2b12 vgabios.c:1849
    inc si                                    ; 46                          ; 0xc2b14
    mov dx, si                                ; 89 f2                       ; 0xc2b15
    in AL, DX                                 ; ec                          ; 0xc2b17
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b18
    mov di, ax                                ; 89 c7                       ; 0xc2b1a
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2b1c vgabios.c:1850
    mov dx, cx                                ; 89 ca                       ; 0xc2b1e
    out DX, AL                                ; ee                          ; 0xc2b20
    mov dx, si                                ; 89 f2                       ; 0xc2b21 vgabios.c:1851
    in AL, DX                                 ; ec                          ; 0xc2b23
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b24
    mov dl, al                                ; 88 c2                       ; 0xc2b26 vgabios.c:1852
    and dl, 002h                              ; 80 e2 02                    ; 0xc2b28
    xor dh, dh                                ; 30 f6                       ; 0xc2b2b
    sal dx, 007h                              ; c1 e2 07                    ; 0xc2b2d
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2b30
    xor ah, ah                                ; 30 e4                       ; 0xc2b32
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2b34
    add ax, dx                                ; 01 d0                       ; 0xc2b37
    inc ax                                    ; 40                          ; 0xc2b39
    add ax, di                                ; 01 f8                       ; 0xc2b3a
    xor dx, dx                                ; 31 d2                       ; 0xc2b3c vgabios.c:1853
    div bx                                    ; f7 f3                       ; 0xc2b3e
    mov dl, al                                ; 88 c2                       ; 0xc2b40 vgabios.c:1854
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2b42
    mov si, 00084h                            ; be 84 00                    ; 0xc2b44 vgabios.c:42
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2b47
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2b4a vgabios.c:47
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2b4d
    xor ah, ah                                ; 30 e4                       ; 0xc2b50 vgabios.c:1856
    mul dx                                    ; f7 e2                       ; 0xc2b52
    add ax, ax                                ; 01 c0                       ; 0xc2b54
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2b56 vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2b59
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2b5c vgabios.c:1857
    pop di                                    ; 5f                          ; 0xc2b5f
    pop si                                    ; 5e                          ; 0xc2b60
    pop dx                                    ; 5a                          ; 0xc2b61
    pop cx                                    ; 59                          ; 0xc2b62
    pop bx                                    ; 5b                          ; 0xc2b63
    pop bp                                    ; 5d                          ; 0xc2b64
    retn                                      ; c3                          ; 0xc2b65
  ; disGetNextSymbol 0xc2b66 LB 0x1709 -> off=0x0 cb=000000000000007f uValue=00000000000c2b66 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2b66 LB 0x7f
    push bp                                   ; 55                          ; 0xc2b66 vgabios.c:1859
    mov bp, sp                                ; 89 e5                       ; 0xc2b67
    push si                                   ; 56                          ; 0xc2b69
    push di                                   ; 57                          ; 0xc2b6a
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2b6b
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2b6e
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc2b71
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2b74
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc2b77
    call 02a4dh                               ; e8 d0 fe                    ; 0xc2b7a vgabios.c:1864
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2b7d vgabios.c:1865
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2b80
    xor ah, ah                                ; 30 e4                       ; 0xc2b82
    mov bx, ax                                ; 89 c3                       ; 0xc2b84
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2b86
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2b89
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2b8c
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2b8e
    add bx, ax                                ; 01 c3                       ; 0xc2b91
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2b93
    xor bx, bx                                ; 31 db                       ; 0xc2b96 vgabios.c:1866
    cmp bx, word [bp-00eh]                    ; 3b 5e f2                    ; 0xc2b98
    jnc short 02bcbh                          ; 73 2e                       ; 0xc2b9b
    mov cl, byte [bp+008h]                    ; 8a 4e 08                    ; 0xc2b9d vgabios.c:1868
    xor ch, ch                                ; 30 ed                       ; 0xc2ba0
    mov ax, bx                                ; 89 d8                       ; 0xc2ba2
    mul cx                                    ; f7 e1                       ; 0xc2ba4
    mov si, word [bp-00ah]                    ; 8b 76 f6                    ; 0xc2ba6
    add si, ax                                ; 01 c6                       ; 0xc2ba9
    mov ax, word [bp+004h]                    ; 8b 46 04                    ; 0xc2bab vgabios.c:1869
    add ax, bx                                ; 01 d8                       ; 0xc2bae
    sal ax, 005h                              ; c1 e0 05                    ; 0xc2bb0
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc2bb3
    add di, ax                                ; 01 c7                       ; 0xc2bb6
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc2bb8 vgabios.c:1870
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2bbb
    mov es, ax                                ; 8e c0                       ; 0xc2bbe
    jcxz 02bc8h                               ; e3 06                       ; 0xc2bc0
    push DS                                   ; 1e                          ; 0xc2bc2
    mov ds, dx                                ; 8e da                       ; 0xc2bc3
    rep movsb                                 ; f3 a4                       ; 0xc2bc5
    pop DS                                    ; 1f                          ; 0xc2bc7
    inc bx                                    ; 43                          ; 0xc2bc8 vgabios.c:1871
    jmp short 02b98h                          ; eb cd                       ; 0xc2bc9
    call 02a79h                               ; e8 ab fe                    ; 0xc2bcb vgabios.c:1872
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2bce vgabios.c:1873
    jc short 02bdch                           ; 72 08                       ; 0xc2bd2
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2bd4 vgabios.c:1875
    xor ah, ah                                ; 30 e4                       ; 0xc2bd7
    call 02ab5h                               ; e8 d9 fe                    ; 0xc2bd9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2bdc vgabios.c:1877
    pop di                                    ; 5f                          ; 0xc2bdf
    pop si                                    ; 5e                          ; 0xc2be0
    pop bp                                    ; 5d                          ; 0xc2be1
    retn 00006h                               ; c2 06 00                    ; 0xc2be2
  ; disGetNextSymbol 0xc2be5 LB 0x168a -> off=0x0 cb=000000000000006d uValue=00000000000c2be5 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2be5 LB 0x6d
    push bp                                   ; 55                          ; 0xc2be5 vgabios.c:1879
    mov bp, sp                                ; 89 e5                       ; 0xc2be6
    push bx                                   ; 53                          ; 0xc2be8
    push cx                                   ; 51                          ; 0xc2be9
    push si                                   ; 56                          ; 0xc2bea
    push di                                   ; 57                          ; 0xc2beb
    push ax                                   ; 50                          ; 0xc2bec
    push ax                                   ; 50                          ; 0xc2bed
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2bee
    call 02a4dh                               ; e8 59 fe                    ; 0xc2bf1 vgabios.c:1883
    mov al, dl                                ; 88 d0                       ; 0xc2bf4 vgabios.c:1884
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2bf6
    xor ah, ah                                ; 30 e4                       ; 0xc2bf8
    mov bx, ax                                ; 89 c3                       ; 0xc2bfa
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2bfc
    mov al, dl                                ; 88 d0                       ; 0xc2bff
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c01
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c03
    add bx, ax                                ; 01 c3                       ; 0xc2c06
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c08
    xor bx, bx                                ; 31 db                       ; 0xc2c0b vgabios.c:1885
    jmp short 02c15h                          ; eb 06                       ; 0xc2c0d
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c0f
    jnc short 02c3ah                          ; 73 25                       ; 0xc2c13
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2c15 vgabios.c:1887
    mov di, bx                                ; 89 df                       ; 0xc2c18 vgabios.c:1888
    sal di, 005h                              ; c1 e7 05                    ; 0xc2c1a
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c1d
    add si, 05d6ch                            ; 81 c6 6c 5d                 ; 0xc2c20 vgabios.c:1889
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2c24
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2c27
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c2a
    mov es, ax                                ; 8e c0                       ; 0xc2c2d
    jcxz 02c37h                               ; e3 06                       ; 0xc2c2f
    push DS                                   ; 1e                          ; 0xc2c31
    mov ds, dx                                ; 8e da                       ; 0xc2c32
    rep movsb                                 ; f3 a4                       ; 0xc2c34
    pop DS                                    ; 1f                          ; 0xc2c36
    inc bx                                    ; 43                          ; 0xc2c37 vgabios.c:1890
    jmp short 02c0fh                          ; eb d5                       ; 0xc2c38
    call 02a79h                               ; e8 3c fe                    ; 0xc2c3a vgabios.c:1891
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2c3d vgabios.c:1892
    jc short 02c49h                           ; 72 06                       ; 0xc2c41
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2c43 vgabios.c:1894
    call 02ab5h                               ; e8 6c fe                    ; 0xc2c46
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2c49 vgabios.c:1896
    pop di                                    ; 5f                          ; 0xc2c4c
    pop si                                    ; 5e                          ; 0xc2c4d
    pop cx                                    ; 59                          ; 0xc2c4e
    pop bx                                    ; 5b                          ; 0xc2c4f
    pop bp                                    ; 5d                          ; 0xc2c50
    retn                                      ; c3                          ; 0xc2c51
  ; disGetNextSymbol 0xc2c52 LB 0x161d -> off=0x0 cb=000000000000006f uValue=00000000000c2c52 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2c52 LB 0x6f
    push bp                                   ; 55                          ; 0xc2c52 vgabios.c:1898
    mov bp, sp                                ; 89 e5                       ; 0xc2c53
    push bx                                   ; 53                          ; 0xc2c55
    push cx                                   ; 51                          ; 0xc2c56
    push si                                   ; 56                          ; 0xc2c57
    push di                                   ; 57                          ; 0xc2c58
    push ax                                   ; 50                          ; 0xc2c59
    push ax                                   ; 50                          ; 0xc2c5a
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c5b
    call 02a4dh                               ; e8 ec fd                    ; 0xc2c5e vgabios.c:1902
    mov al, dl                                ; 88 d0                       ; 0xc2c61 vgabios.c:1903
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c63
    xor ah, ah                                ; 30 e4                       ; 0xc2c65
    mov bx, ax                                ; 89 c3                       ; 0xc2c67
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2c69
    mov al, dl                                ; 88 d0                       ; 0xc2c6c
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c6e
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c70
    add bx, ax                                ; 01 c3                       ; 0xc2c73
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c75
    xor bx, bx                                ; 31 db                       ; 0xc2c78 vgabios.c:1904
    jmp short 02c82h                          ; eb 06                       ; 0xc2c7a
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c7c
    jnc short 02ca9h                          ; 73 27                       ; 0xc2c80
    mov si, bx                                ; 89 de                       ; 0xc2c82 vgabios.c:1906
    sal si, 003h                              ; c1 e6 03                    ; 0xc2c84
    mov di, bx                                ; 89 df                       ; 0xc2c87 vgabios.c:1907
    sal di, 005h                              ; c1 e7 05                    ; 0xc2c89
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c8c
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2c8f vgabios.c:1908
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2c93
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2c96
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c99
    mov es, ax                                ; 8e c0                       ; 0xc2c9c
    jcxz 02ca6h                               ; e3 06                       ; 0xc2c9e
    push DS                                   ; 1e                          ; 0xc2ca0
    mov ds, dx                                ; 8e da                       ; 0xc2ca1
    rep movsb                                 ; f3 a4                       ; 0xc2ca3
    pop DS                                    ; 1f                          ; 0xc2ca5
    inc bx                                    ; 43                          ; 0xc2ca6 vgabios.c:1909
    jmp short 02c7ch                          ; eb d3                       ; 0xc2ca7
    call 02a79h                               ; e8 cd fd                    ; 0xc2ca9 vgabios.c:1910
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2cac vgabios.c:1911
    jc short 02cb8h                           ; 72 06                       ; 0xc2cb0
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2cb2 vgabios.c:1913
    call 02ab5h                               ; e8 fd fd                    ; 0xc2cb5
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2cb8 vgabios.c:1915
    pop di                                    ; 5f                          ; 0xc2cbb
    pop si                                    ; 5e                          ; 0xc2cbc
    pop cx                                    ; 59                          ; 0xc2cbd
    pop bx                                    ; 5b                          ; 0xc2cbe
    pop bp                                    ; 5d                          ; 0xc2cbf
    retn                                      ; c3                          ; 0xc2cc0
  ; disGetNextSymbol 0xc2cc1 LB 0x15ae -> off=0x0 cb=000000000000006f uValue=00000000000c2cc1 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2cc1 LB 0x6f
    push bp                                   ; 55                          ; 0xc2cc1 vgabios.c:1918
    mov bp, sp                                ; 89 e5                       ; 0xc2cc2
    push bx                                   ; 53                          ; 0xc2cc4
    push cx                                   ; 51                          ; 0xc2cc5
    push si                                   ; 56                          ; 0xc2cc6
    push di                                   ; 57                          ; 0xc2cc7
    push ax                                   ; 50                          ; 0xc2cc8
    push ax                                   ; 50                          ; 0xc2cc9
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2cca
    call 02a4dh                               ; e8 7d fd                    ; 0xc2ccd vgabios.c:1922
    mov al, dl                                ; 88 d0                       ; 0xc2cd0 vgabios.c:1923
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2cd2
    xor ah, ah                                ; 30 e4                       ; 0xc2cd4
    mov bx, ax                                ; 89 c3                       ; 0xc2cd6
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2cd8
    mov al, dl                                ; 88 d0                       ; 0xc2cdb
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2cdd
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2cdf
    add bx, ax                                ; 01 c3                       ; 0xc2ce2
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2ce4
    xor bx, bx                                ; 31 db                       ; 0xc2ce7 vgabios.c:1924
    jmp short 02cf1h                          ; eb 06                       ; 0xc2ce9
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2ceb
    jnc short 02d18h                          ; 73 27                       ; 0xc2cef
    mov si, bx                                ; 89 de                       ; 0xc2cf1 vgabios.c:1926
    sal si, 004h                              ; c1 e6 04                    ; 0xc2cf3
    mov di, bx                                ; 89 df                       ; 0xc2cf6 vgabios.c:1927
    sal di, 005h                              ; c1 e7 05                    ; 0xc2cf8
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2cfb
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2cfe vgabios.c:1928
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2d02
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d05
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d08
    mov es, ax                                ; 8e c0                       ; 0xc2d0b
    jcxz 02d15h                               ; e3 06                       ; 0xc2d0d
    push DS                                   ; 1e                          ; 0xc2d0f
    mov ds, dx                                ; 8e da                       ; 0xc2d10
    rep movsb                                 ; f3 a4                       ; 0xc2d12
    pop DS                                    ; 1f                          ; 0xc2d14
    inc bx                                    ; 43                          ; 0xc2d15 vgabios.c:1929
    jmp short 02cebh                          ; eb d3                       ; 0xc2d16
    call 02a79h                               ; e8 5e fd                    ; 0xc2d18 vgabios.c:1930
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2d1b vgabios.c:1931
    jc short 02d27h                           ; 72 06                       ; 0xc2d1f
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2d21 vgabios.c:1933
    call 02ab5h                               ; e8 8e fd                    ; 0xc2d24
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2d27 vgabios.c:1935
    pop di                                    ; 5f                          ; 0xc2d2a
    pop si                                    ; 5e                          ; 0xc2d2b
    pop cx                                    ; 59                          ; 0xc2d2c
    pop bx                                    ; 5b                          ; 0xc2d2d
    pop bp                                    ; 5d                          ; 0xc2d2e
    retn                                      ; c3                          ; 0xc2d2f
  ; disGetNextSymbol 0xc2d30 LB 0x153f -> off=0x0 cb=0000000000000005 uValue=00000000000c2d30 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2d30 LB 0x5
    push bp                                   ; 55                          ; 0xc2d30 vgabios.c:1937
    mov bp, sp                                ; 89 e5                       ; 0xc2d31
    pop bp                                    ; 5d                          ; 0xc2d33 vgabios.c:1942
    retn                                      ; c3                          ; 0xc2d34
  ; disGetNextSymbol 0xc2d35 LB 0x153a -> off=0x0 cb=0000000000000007 uValue=00000000000c2d35 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2d35 LB 0x7
    push bp                                   ; 55                          ; 0xc2d35 vgabios.c:1943
    mov bp, sp                                ; 89 e5                       ; 0xc2d36
    pop bp                                    ; 5d                          ; 0xc2d38 vgabios.c:1949
    retn 00002h                               ; c2 02 00                    ; 0xc2d39
  ; disGetNextSymbol 0xc2d3c LB 0x1533 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d3c 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2d3c LB 0x5
    push bp                                   ; 55                          ; 0xc2d3c vgabios.c:1950
    mov bp, sp                                ; 89 e5                       ; 0xc2d3d
    pop bp                                    ; 5d                          ; 0xc2d3f vgabios.c:1955
    retn                                      ; c3                          ; 0xc2d40
  ; disGetNextSymbol 0xc2d41 LB 0x152e -> off=0x0 cb=0000000000000005 uValue=00000000000c2d41 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2d41 LB 0x5
    push bp                                   ; 55                          ; 0xc2d41 vgabios.c:1956
    mov bp, sp                                ; 89 e5                       ; 0xc2d42
    pop bp                                    ; 5d                          ; 0xc2d44 vgabios.c:1961
    retn                                      ; c3                          ; 0xc2d45
  ; disGetNextSymbol 0xc2d46 LB 0x1529 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d46 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2d46 LB 0x5
    push bp                                   ; 55                          ; 0xc2d46 vgabios.c:1962
    mov bp, sp                                ; 89 e5                       ; 0xc2d47
    pop bp                                    ; 5d                          ; 0xc2d49 vgabios.c:1967
    retn                                      ; c3                          ; 0xc2d4a
  ; disGetNextSymbol 0xc2d4b LB 0x1524 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d4b 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2d4b LB 0x5
    push bp                                   ; 55                          ; 0xc2d4b vgabios.c:1969
    mov bp, sp                                ; 89 e5                       ; 0xc2d4c
    pop bp                                    ; 5d                          ; 0xc2d4e vgabios.c:1974
    retn                                      ; c3                          ; 0xc2d4f
  ; disGetNextSymbol 0xc2d50 LB 0x151f -> off=0x0 cb=0000000000000005 uValue=00000000000c2d50 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2d50 LB 0x5
    push bp                                   ; 55                          ; 0xc2d50 vgabios.c:1977
    mov bp, sp                                ; 89 e5                       ; 0xc2d51
    pop bp                                    ; 5d                          ; 0xc2d53 vgabios.c:1982
    retn                                      ; c3                          ; 0xc2d54
  ; disGetNextSymbol 0xc2d55 LB 0x151a -> off=0x0 cb=0000000000000005 uValue=00000000000c2d55 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2d55 LB 0x5
    push bp                                   ; 55                          ; 0xc2d55 vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2d56
    pop bp                                    ; 5d                          ; 0xc2d58 vgabios.c:1988
    retn                                      ; c3                          ; 0xc2d59
  ; disGetNextSymbol 0xc2d5a LB 0x1515 -> off=0x0 cb=000000000000009d uValue=00000000000c2d5a 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2d5a LB 0x9d
    push bp                                   ; 55                          ; 0xc2d5a vgabios.c:1991
    mov bp, sp                                ; 89 e5                       ; 0xc2d5b
    push si                                   ; 56                          ; 0xc2d5d
    push di                                   ; 57                          ; 0xc2d5e
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2d5f
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2d62
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc2d65
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2d68
    mov si, cx                                ; 89 ce                       ; 0xc2d6b
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2d6d
    mov al, dl                                ; 88 d0                       ; 0xc2d70 vgabios.c:1998
    xor ah, ah                                ; 30 e4                       ; 0xc2d72
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2d74
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2d77
    call 00a1ah                               ; e8 9d dc                    ; 0xc2d7a
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2d7d vgabios.c:2001
    jne short 02d94h                          ; 75 11                       ; 0xc2d81
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2d83 vgabios.c:2002
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2d86
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2d89 vgabios.c:2003
    xor al, al                                ; 30 c0                       ; 0xc2d8c
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2d8e
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2d91
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc2d94 vgabios.c:2006
    xor dh, dh                                ; 30 f6                       ; 0xc2d97
    sal dx, 008h                              ; c1 e2 08                    ; 0xc2d99
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2d9c
    xor ah, ah                                ; 30 e4                       ; 0xc2d9f
    add dx, ax                                ; 01 c2                       ; 0xc2da1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2da3 vgabios.c:2007
    call 01217h                               ; e8 6e e4                    ; 0xc2da6
    dec si                                    ; 4e                          ; 0xc2da9 vgabios.c:2009
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2daa
    je short 02dddh                           ; 74 2e                       ; 0xc2dad
    mov bx, di                                ; 89 fb                       ; 0xc2daf vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2db1
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2db2 vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2db5
    test byte [bp-006h], 002h                 ; f6 46 fa 02                 ; 0xc2db8 vgabios.c:2012
    je short 02dc7h                           ; 74 09                       ; 0xc2dbc
    mov bx, di                                ; 89 fb                       ; 0xc2dbe vgabios.c:2013
    inc di                                    ; 47                          ; 0xc2dc0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2dc1 vgabios.c:37
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2dc4 vgabios.c:38
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2dc7 vgabios.c:2015
    xor bh, bh                                ; 30 ff                       ; 0xc2dca
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2dcc
    xor dh, dh                                ; 30 f6                       ; 0xc2dcf
    mov al, ah                                ; 88 e0                       ; 0xc2dd1
    xor ah, ah                                ; 30 e4                       ; 0xc2dd3
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2dd5
    call 027f5h                               ; e8 1a fa                    ; 0xc2dd8
    jmp short 02da9h                          ; eb cc                       ; 0xc2ddb vgabios.c:2016
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc2ddd vgabios.c:2019
    jne short 02deeh                          ; 75 0b                       ; 0xc2de1
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2de3 vgabios.c:2020
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2de6
    xor ah, ah                                ; 30 e4                       ; 0xc2de9
    call 01217h                               ; e8 29 e4                    ; 0xc2deb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2dee vgabios.c:2021
    pop di                                    ; 5f                          ; 0xc2df1
    pop si                                    ; 5e                          ; 0xc2df2
    pop bp                                    ; 5d                          ; 0xc2df3
    retn 00008h                               ; c2 08 00                    ; 0xc2df4
  ; disGetNextSymbol 0xc2df7 LB 0x1478 -> off=0x0 cb=00000000000001ef uValue=00000000000c2df7 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2df7 LB 0x1ef
    push bp                                   ; 55                          ; 0xc2df7 vgabios.c:2024
    mov bp, sp                                ; 89 e5                       ; 0xc2df8
    push cx                                   ; 51                          ; 0xc2dfa
    push si                                   ; 56                          ; 0xc2dfb
    push di                                   ; 57                          ; 0xc2dfc
    push ax                                   ; 50                          ; 0xc2dfd
    push ax                                   ; 50                          ; 0xc2dfe
    push dx                                   ; 52                          ; 0xc2dff
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e00 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e03
    mov es, ax                                ; 8e c0                       ; 0xc2e06
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e08
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2e0b vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2e0e vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2e11
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e14 vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2e17 vgabios.c:2035
    mov es, dx                                ; 8e c2                       ; 0xc2e19 vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2e1b
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2e20
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2e24 vgabios.c:2040
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2e27
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e2a
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2e2d
    jcxz 02e38h                               ; e3 06                       ; 0xc2e30
    push DS                                   ; 1e                          ; 0xc2e32
    mov ds, dx                                ; 8e da                       ; 0xc2e33
    rep movsb                                 ; f3 a4                       ; 0xc2e35
    pop DS                                    ; 1f                          ; 0xc2e37
    mov si, 00084h                            ; be 84 00                    ; 0xc2e38 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e3b
    mov es, ax                                ; 8e c0                       ; 0xc2e3e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e40
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2e43 vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2e45
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2e48 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2e4b
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2e4e vgabios.c:2042
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2e51
    mov si, 00085h                            ; be 85 00                    ; 0xc2e54
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2e57
    jcxz 02e62h                               ; e3 06                       ; 0xc2e5a
    push DS                                   ; 1e                          ; 0xc2e5c
    mov ds, dx                                ; 8e da                       ; 0xc2e5d
    rep movsb                                 ; f3 a4                       ; 0xc2e5f
    pop DS                                    ; 1f                          ; 0xc2e61
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2e62 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e65
    mov es, ax                                ; 8e c0                       ; 0xc2e68
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e6a
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2e6d vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2e70 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2e73
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2e76 vgabios.c:2045
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2e79 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2e7d vgabios.c:2046
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2e80 vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2e85 vgabios.c:2047
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2e88 vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2e8c vgabios.c:2048
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2e8f vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2e93 vgabios.c:2049
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2e96 vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2e9a vgabios.c:2050
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2e9d vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2ea1 vgabios.c:2051
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2ea4 vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2ea8 vgabios.c:2052
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2eab vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2eaf vgabios.c:2053
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2eb2 vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2eb6 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2eb9
    mov es, ax                                ; 8e c0                       ; 0xc2ebc
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2ebe
    mov dl, al                                ; 88 c2                       ; 0xc2ec1 vgabios.c:2058
    and dl, 080h                              ; 80 e2 80                    ; 0xc2ec3
    xor dh, dh                                ; 30 f6                       ; 0xc2ec6
    sar dx, 006h                              ; c1 fa 06                    ; 0xc2ec8
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2ecb
    xor ah, ah                                ; 30 e4                       ; 0xc2ecd
    sar ax, 004h                              ; c1 f8 04                    ; 0xc2ecf
    or ax, dx                                 ; 09 d0                       ; 0xc2ed2
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2ed4 vgabios.c:2059
    je short 02eeah                           ; 74 11                       ; 0xc2ed7
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2ed9
    je short 02ee6h                           ; 74 08                       ; 0xc2edc
    test ax, ax                               ; 85 c0                       ; 0xc2ede
    jne short 02eeah                          ; 75 08                       ; 0xc2ee0
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2ee2 vgabios.c:2060
    jmp short 02eech                          ; eb 06                       ; 0xc2ee4
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2ee6 vgabios.c:2061
    jmp short 02eech                          ; eb 02                       ; 0xc2ee8
    xor al, al                                ; 30 c0                       ; 0xc2eea vgabios.c:2063
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2eec vgabios.c:2065
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2eef vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2ef2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ef5 vgabios.c:2068
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2ef8
    jc short 02f1bh                           ; 72 1f                       ; 0xc2efa
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2efc
    jnbe short 02f1bh                         ; 77 1b                       ; 0xc2efe
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2f00 vgabios.c:2069
    test ax, ax                               ; 85 c0                       ; 0xc2f03
    je short 02f5dh                           ; 74 56                       ; 0xc2f05
    mov si, ax                                ; 89 c6                       ; 0xc2f07 vgabios.c:2070
    shr si, 002h                              ; c1 ee 02                    ; 0xc2f09
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2f0c
    xor dx, dx                                ; 31 d2                       ; 0xc2f0f
    div si                                    ; f7 f6                       ; 0xc2f11
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f13
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f16 vgabios.c:42
    jmp short 02f5dh                          ; eb 42                       ; 0xc2f19 vgabios.c:2071
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f1b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f1e
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2f21
    jne short 02f36h                          ; 75 11                       ; 0xc2f23
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f25 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2f28
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f2c vgabios.c:2073
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2f2f vgabios.c:52
    jmp short 02f5dh                          ; eb 27                       ; 0xc2f34 vgabios.c:2074
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2f36
    jc short 02f5dh                           ; 72 23                       ; 0xc2f38
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2f3a
    jnbe short 02f5dh                         ; 77 1f                       ; 0xc2f3c
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2f3e vgabios.c:2076
    je short 02f52h                           ; 74 0e                       ; 0xc2f42
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2f44 vgabios.c:2077
    xor dx, dx                                ; 31 d2                       ; 0xc2f47
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2f49
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f4c vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f4f
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f52 vgabios.c:2078
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f55 vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2f58
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f5d vgabios.c:2080
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2f60
    je short 02f68h                           ; 74 04                       ; 0xc2f62
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2f64
    jne short 02f73h                          ; 75 0b                       ; 0xc2f66
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f68 vgabios.c:2081
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f6b vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc2f6e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f73 vgabios.c:2083
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2f76
    jc short 02fcfh                           ; 72 55                       ; 0xc2f78
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc2f7a
    je short 02fcfh                           ; 74 51                       ; 0xc2f7c
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2f7e vgabios.c:2084
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f81 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2f84
    mov si, 00084h                            ; be 84 00                    ; 0xc2f88 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f8b
    mov es, ax                                ; 8e c0                       ; 0xc2f8e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f90
    xor ah, ah                                ; 30 e4                       ; 0xc2f93 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc2f95
    mov si, 00085h                            ; be 85 00                    ; 0xc2f96 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2f99
    xor dh, dh                                ; 30 f6                       ; 0xc2f9c vgabios.c:38
    imul dx                                   ; f7 ea                       ; 0xc2f9e
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc2fa0 vgabios.c:2086
    jc short 02fb3h                           ; 72 0e                       ; 0xc2fa3
    jbe short 02fbch                          ; 76 15                       ; 0xc2fa5
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc2fa7
    je short 02fc4h                           ; 74 18                       ; 0xc2faa
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc2fac
    je short 02fc0h                           ; 74 0f                       ; 0xc2faf
    jmp short 02fc4h                          ; eb 11                       ; 0xc2fb1
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc2fb3
    jne short 02fc4h                          ; 75 0c                       ; 0xc2fb6
    xor al, al                                ; 30 c0                       ; 0xc2fb8 vgabios.c:2087
    jmp short 02fc6h                          ; eb 0a                       ; 0xc2fba
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2fbc vgabios.c:2088
    jmp short 02fc6h                          ; eb 06                       ; 0xc2fbe
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2fc0 vgabios.c:2089
    jmp short 02fc6h                          ; eb 02                       ; 0xc2fc2
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2fc4 vgabios.c:2091
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2fc6 vgabios.c:2093
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fc9 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fcc
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc2fcf vgabios.c:2096
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2fd2
    xor ax, ax                                ; 31 c0                       ; 0xc2fd5
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fd7
    jcxz 02fdeh                               ; e3 02                       ; 0xc2fda
    rep stosb                                 ; f3 aa                       ; 0xc2fdc
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2fde vgabios.c:2097
    pop di                                    ; 5f                          ; 0xc2fe1
    pop si                                    ; 5e                          ; 0xc2fe2
    pop cx                                    ; 59                          ; 0xc2fe3
    pop bp                                    ; 5d                          ; 0xc2fe4
    retn                                      ; c3                          ; 0xc2fe5
  ; disGetNextSymbol 0xc2fe6 LB 0x1289 -> off=0x0 cb=0000000000000023 uValue=00000000000c2fe6 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc2fe6 LB 0x23
    push dx                                   ; 52                          ; 0xc2fe6 vgabios.c:2100
    push bp                                   ; 55                          ; 0xc2fe7
    mov bp, sp                                ; 89 e5                       ; 0xc2fe8
    mov dx, ax                                ; 89 c2                       ; 0xc2fea
    xor ax, ax                                ; 31 c0                       ; 0xc2fec vgabios.c:2104
    test dl, 001h                             ; f6 c2 01                    ; 0xc2fee vgabios.c:2105
    je short 02ff6h                           ; 74 03                       ; 0xc2ff1
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2ff3 vgabios.c:2106
    test dl, 002h                             ; f6 c2 02                    ; 0xc2ff6 vgabios.c:2108
    je short 02ffeh                           ; 74 03                       ; 0xc2ff9
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2ffb vgabios.c:2109
    test dl, 004h                             ; f6 c2 04                    ; 0xc2ffe vgabios.c:2111
    je short 03006h                           ; 74 03                       ; 0xc3001
    add ax, 00304h                            ; 05 04 03                    ; 0xc3003 vgabios.c:2112
    pop bp                                    ; 5d                          ; 0xc3006 vgabios.c:2115
    pop dx                                    ; 5a                          ; 0xc3007
    retn                                      ; c3                          ; 0xc3008
  ; disGetNextSymbol 0xc3009 LB 0x1266 -> off=0x0 cb=0000000000000018 uValue=00000000000c3009 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3009 LB 0x18
    push bp                                   ; 55                          ; 0xc3009 vgabios.c:2117
    mov bp, sp                                ; 89 e5                       ; 0xc300a
    push bx                                   ; 53                          ; 0xc300c
    mov bx, dx                                ; 89 d3                       ; 0xc300d
    call 02fe6h                               ; e8 d4 ff                    ; 0xc300f vgabios.c:2120
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3012
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3015
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc3018
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc301b vgabios.c:2121
    pop bx                                    ; 5b                          ; 0xc301e
    pop bp                                    ; 5d                          ; 0xc301f
    retn                                      ; c3                          ; 0xc3020
  ; disGetNextSymbol 0xc3021 LB 0x124e -> off=0x0 cb=00000000000002d8 uValue=00000000000c3021 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc3021 LB 0x2d8
    push bp                                   ; 55                          ; 0xc3021 vgabios.c:2123
    mov bp, sp                                ; 89 e5                       ; 0xc3022
    push cx                                   ; 51                          ; 0xc3024
    push si                                   ; 56                          ; 0xc3025
    push di                                   ; 57                          ; 0xc3026
    push ax                                   ; 50                          ; 0xc3027
    push ax                                   ; 50                          ; 0xc3028
    push ax                                   ; 50                          ; 0xc3029
    mov cx, dx                                ; 89 d1                       ; 0xc302a
    mov si, strict word 00063h                ; be 63 00                    ; 0xc302c vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc302f
    mov es, ax                                ; 8e c0                       ; 0xc3032
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc3034
    mov si, di                                ; 89 fe                       ; 0xc3037 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc3039 vgabios.c:2128
    je short 030a5h                           ; 74 66                       ; 0xc303d
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc303f vgabios.c:2129
    in AL, DX                                 ; ec                          ; 0xc3042
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3043
    mov es, cx                                ; 8e c1                       ; 0xc3045 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3047
    inc bx                                    ; 43                          ; 0xc304a vgabios.c:2129
    mov dx, di                                ; 89 fa                       ; 0xc304b
    in AL, DX                                 ; ec                          ; 0xc304d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc304e
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3050 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3053 vgabios.c:2130
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3054
    in AL, DX                                 ; ec                          ; 0xc3057
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3058
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc305a vgabios.c:42
    inc bx                                    ; 43                          ; 0xc305d vgabios.c:2131
    mov dx, 003dah                            ; ba da 03                    ; 0xc305e
    in AL, DX                                 ; ec                          ; 0xc3061
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3062
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3064 vgabios.c:2133
    in AL, DX                                 ; ec                          ; 0xc3067
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3068
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc306a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc306d vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3070
    inc bx                                    ; 43                          ; 0xc3073 vgabios.c:2134
    mov dx, 003cah                            ; ba ca 03                    ; 0xc3074
    in AL, DX                                 ; ec                          ; 0xc3077
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3078
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc307a vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc307d vgabios.c:2137
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3080
    add bx, ax                                ; 01 c3                       ; 0xc3083 vgabios.c:2135
    jmp short 0308dh                          ; eb 06                       ; 0xc3085
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3087
    jnbe short 030a8h                         ; 77 1b                       ; 0xc308b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc308d vgabios.c:2138
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3090
    out DX, AL                                ; ee                          ; 0xc3093
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3094 vgabios.c:2139
    in AL, DX                                 ; ec                          ; 0xc3097
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3098
    mov es, cx                                ; 8e c1                       ; 0xc309a vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc309c
    inc bx                                    ; 43                          ; 0xc309f vgabios.c:2139
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30a0 vgabios.c:2140
    jmp short 03087h                          ; eb e2                       ; 0xc30a3
    jmp near 03155h                           ; e9 ad 00                    ; 0xc30a5
    xor al, al                                ; 30 c0                       ; 0xc30a8 vgabios.c:2141
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc30aa
    out DX, AL                                ; ee                          ; 0xc30ad
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc30ae vgabios.c:2142
    in AL, DX                                 ; ec                          ; 0xc30b1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30b2
    mov es, cx                                ; 8e c1                       ; 0xc30b4 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30b6
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc30b9 vgabios.c:2144
    inc bx                                    ; 43                          ; 0xc30be vgabios.c:2142
    jmp short 030c7h                          ; eb 06                       ; 0xc30bf
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc30c1
    jnbe short 030deh                         ; 77 17                       ; 0xc30c5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc30c7 vgabios.c:2145
    mov dx, si                                ; 89 f2                       ; 0xc30ca
    out DX, AL                                ; ee                          ; 0xc30cc
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc30cd vgabios.c:2146
    in AL, DX                                 ; ec                          ; 0xc30d0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30d1
    mov es, cx                                ; 8e c1                       ; 0xc30d3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30d5
    inc bx                                    ; 43                          ; 0xc30d8 vgabios.c:2146
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30d9 vgabios.c:2147
    jmp short 030c1h                          ; eb e3                       ; 0xc30dc
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc30de vgabios.c:2149
    jmp short 030ebh                          ; eb 06                       ; 0xc30e3
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc30e5
    jnbe short 0310fh                         ; 77 24                       ; 0xc30e9
    mov dx, 003dah                            ; ba da 03                    ; 0xc30eb vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc30ee
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30ef
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc30f1 vgabios.c:2151
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc30f4
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc30f7
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc30fa
    out DX, AL                                ; ee                          ; 0xc30fd
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc30fe vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc3101
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3102
    mov es, cx                                ; 8e c1                       ; 0xc3104 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3106
    inc bx                                    ; 43                          ; 0xc3109 vgabios.c:2152
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc310a vgabios.c:2153
    jmp short 030e5h                          ; eb d6                       ; 0xc310d
    mov dx, 003dah                            ; ba da 03                    ; 0xc310f vgabios.c:2154
    in AL, DX                                 ; ec                          ; 0xc3112
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3113
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3115 vgabios.c:2156
    jmp short 03122h                          ; eb 06                       ; 0xc311a
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc311c
    jnbe short 0313ah                         ; 77 18                       ; 0xc3120
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3122 vgabios.c:2157
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3125
    out DX, AL                                ; ee                          ; 0xc3128
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc3129 vgabios.c:2158
    in AL, DX                                 ; ec                          ; 0xc312c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc312d
    mov es, cx                                ; 8e c1                       ; 0xc312f vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3131
    inc bx                                    ; 43                          ; 0xc3134 vgabios.c:2158
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3135 vgabios.c:2159
    jmp short 0311ch                          ; eb e2                       ; 0xc3138
    mov es, cx                                ; 8e c1                       ; 0xc313a vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc313c
    inc bx                                    ; 43                          ; 0xc313f vgabios.c:2161
    inc bx                                    ; 43                          ; 0xc3140
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3141 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3145 vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3146 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc314a vgabios.c:2165
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc314b vgabios.c:42
    inc bx                                    ; 43                          ; 0xc314f vgabios.c:2166
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3150 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3154 vgabios.c:2167
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc3155 vgabios.c:2169
    jne short 0315eh                          ; 75 03                       ; 0xc3159
    jmp near 0329dh                           ; e9 3f 01                    ; 0xc315b
    mov si, strict word 00049h                ; be 49 00                    ; 0xc315e vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3161
    mov es, ax                                ; 8e c0                       ; 0xc3164
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3166
    mov es, cx                                ; 8e c1                       ; 0xc3169 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc316b
    inc bx                                    ; 43                          ; 0xc316e vgabios.c:2170
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc316f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3172
    mov es, ax                                ; 8e c0                       ; 0xc3175
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3177
    mov es, cx                                ; 8e c1                       ; 0xc317a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc317c
    inc bx                                    ; 43                          ; 0xc317f vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc3180
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3181 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3184
    mov es, ax                                ; 8e c0                       ; 0xc3187
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3189
    mov es, cx                                ; 8e c1                       ; 0xc318c vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc318e
    inc bx                                    ; 43                          ; 0xc3191 vgabios.c:2172
    inc bx                                    ; 43                          ; 0xc3192
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3193 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3196
    mov es, ax                                ; 8e c0                       ; 0xc3199
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc319b
    mov es, cx                                ; 8e c1                       ; 0xc319e vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31a0
    inc bx                                    ; 43                          ; 0xc31a3 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc31a4
    mov si, 00084h                            ; be 84 00                    ; 0xc31a5 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31a8
    mov es, ax                                ; 8e c0                       ; 0xc31ab
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31ad
    mov es, cx                                ; 8e c1                       ; 0xc31b0 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31b2
    inc bx                                    ; 43                          ; 0xc31b5 vgabios.c:2174
    mov si, 00085h                            ; be 85 00                    ; 0xc31b6 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31b9
    mov es, ax                                ; 8e c0                       ; 0xc31bc
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc31be
    mov es, cx                                ; 8e c1                       ; 0xc31c1 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31c3
    inc bx                                    ; 43                          ; 0xc31c6 vgabios.c:2175
    inc bx                                    ; 43                          ; 0xc31c7
    mov si, 00087h                            ; be 87 00                    ; 0xc31c8 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31cb
    mov es, ax                                ; 8e c0                       ; 0xc31ce
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31d0
    mov es, cx                                ; 8e c1                       ; 0xc31d3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31d5
    inc bx                                    ; 43                          ; 0xc31d8 vgabios.c:2176
    mov si, 00088h                            ; be 88 00                    ; 0xc31d9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31dc
    mov es, ax                                ; 8e c0                       ; 0xc31df
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31e1
    mov es, cx                                ; 8e c1                       ; 0xc31e4 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31e6
    inc bx                                    ; 43                          ; 0xc31e9 vgabios.c:2177
    mov si, 00089h                            ; be 89 00                    ; 0xc31ea vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31ed
    mov es, ax                                ; 8e c0                       ; 0xc31f0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31f2
    mov es, cx                                ; 8e c1                       ; 0xc31f5 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31f7
    inc bx                                    ; 43                          ; 0xc31fa vgabios.c:2178
    mov si, strict word 00060h                ; be 60 00                    ; 0xc31fb vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31fe
    mov es, ax                                ; 8e c0                       ; 0xc3201
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3203
    mov es, cx                                ; 8e c1                       ; 0xc3206 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3208
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc320b vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc3210 vgabios.c:2179
    inc bx                                    ; 43                          ; 0xc3211
    jmp short 0321ah                          ; eb 06                       ; 0xc3212
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3214
    jnc short 03236h                          ; 73 1c                       ; 0xc3218
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc321a vgabios.c:2181
    add si, si                                ; 01 f6                       ; 0xc321d
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc321f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3222 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc3225
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3227
    mov es, cx                                ; 8e c1                       ; 0xc322a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc322c
    inc bx                                    ; 43                          ; 0xc322f vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc3230
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3231 vgabios.c:2183
    jmp short 03214h                          ; eb de                       ; 0xc3234
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3236 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3239
    mov es, ax                                ; 8e c0                       ; 0xc323c
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc323e
    mov es, cx                                ; 8e c1                       ; 0xc3241 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3243
    inc bx                                    ; 43                          ; 0xc3246 vgabios.c:2184
    inc bx                                    ; 43                          ; 0xc3247
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3248 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc324b
    mov es, ax                                ; 8e c0                       ; 0xc324e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3250
    mov es, cx                                ; 8e c1                       ; 0xc3253 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3255
    inc bx                                    ; 43                          ; 0xc3258 vgabios.c:2185
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3259 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc325c
    mov es, ax                                ; 8e c0                       ; 0xc325e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3260
    mov es, cx                                ; 8e c1                       ; 0xc3263 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3265
    inc bx                                    ; 43                          ; 0xc3268 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc3269
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc326a vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc326d
    mov es, ax                                ; 8e c0                       ; 0xc326f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3271
    mov es, cx                                ; 8e c1                       ; 0xc3274 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3276
    inc bx                                    ; 43                          ; 0xc3279 vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc327a
    mov si, 0010ch                            ; be 0c 01                    ; 0xc327b vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc327e
    mov es, ax                                ; 8e c0                       ; 0xc3280
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3282
    mov es, cx                                ; 8e c1                       ; 0xc3285 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3287
    inc bx                                    ; 43                          ; 0xc328a vgabios.c:2189
    inc bx                                    ; 43                          ; 0xc328b
    mov si, 0010eh                            ; be 0e 01                    ; 0xc328c vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc328f
    mov es, ax                                ; 8e c0                       ; 0xc3291
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3293
    mov es, cx                                ; 8e c1                       ; 0xc3296 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3298
    inc bx                                    ; 43                          ; 0xc329b vgabios.c:2190
    inc bx                                    ; 43                          ; 0xc329c
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc329d vgabios.c:2192
    je short 032efh                           ; 74 4c                       ; 0xc32a1
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc32a3 vgabios.c:2194
    in AL, DX                                 ; ec                          ; 0xc32a6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32a7
    mov es, cx                                ; 8e c1                       ; 0xc32a9 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32ab
    inc bx                                    ; 43                          ; 0xc32ae vgabios.c:2194
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc32af
    in AL, DX                                 ; ec                          ; 0xc32b2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32b3
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32b5 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc32b8 vgabios.c:2195
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc32b9
    in AL, DX                                 ; ec                          ; 0xc32bc
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32bd
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32bf vgabios.c:42
    inc bx                                    ; 43                          ; 0xc32c2 vgabios.c:2196
    xor al, al                                ; 30 c0                       ; 0xc32c3
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc32c5
    out DX, AL                                ; ee                          ; 0xc32c8
    xor ah, ah                                ; 30 e4                       ; 0xc32c9 vgabios.c:2199
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc32cb
    jmp short 032d7h                          ; eb 07                       ; 0xc32ce
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc32d0
    jnc short 032e8h                          ; 73 11                       ; 0xc32d5
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc32d7 vgabios.c:2200
    in AL, DX                                 ; ec                          ; 0xc32da
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32db
    mov es, cx                                ; 8e c1                       ; 0xc32dd vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32df
    inc bx                                    ; 43                          ; 0xc32e2 vgabios.c:2200
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc32e3 vgabios.c:2201
    jmp short 032d0h                          ; eb e8                       ; 0xc32e6
    mov es, cx                                ; 8e c1                       ; 0xc32e8 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc32ea
    inc bx                                    ; 43                          ; 0xc32ee vgabios.c:2202
    mov ax, bx                                ; 89 d8                       ; 0xc32ef vgabios.c:2205
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc32f1
    pop di                                    ; 5f                          ; 0xc32f4
    pop si                                    ; 5e                          ; 0xc32f5
    pop cx                                    ; 59                          ; 0xc32f6
    pop bp                                    ; 5d                          ; 0xc32f7
    retn                                      ; c3                          ; 0xc32f8
  ; disGetNextSymbol 0xc32f9 LB 0xf76 -> off=0x0 cb=00000000000002ba uValue=00000000000c32f9 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc32f9 LB 0x2ba
    push bp                                   ; 55                          ; 0xc32f9 vgabios.c:2207
    mov bp, sp                                ; 89 e5                       ; 0xc32fa
    push cx                                   ; 51                          ; 0xc32fc
    push si                                   ; 56                          ; 0xc32fd
    push di                                   ; 57                          ; 0xc32fe
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc32ff
    push ax                                   ; 50                          ; 0xc3302
    mov cx, dx                                ; 89 d1                       ; 0xc3303
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3305 vgabios.c:2211
    je short 03362h                           ; 74 57                       ; 0xc3309
    mov dx, 003dah                            ; ba da 03                    ; 0xc330b vgabios.c:2213
    in AL, DX                                 ; ec                          ; 0xc330e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc330f
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc3311 vgabios.c:2215
    mov es, cx                                ; 8e c1                       ; 0xc3314 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3316
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3319 vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc331c vgabios.c:2216
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc331e vgabios.c:2219
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc3323 vgabios.c:2217
    jmp short 0332eh                          ; eb 06                       ; 0xc3326
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3328
    jnbe short 03344h                         ; 77 16                       ; 0xc332c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc332e vgabios.c:2220
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3331
    out DX, AL                                ; ee                          ; 0xc3334
    mov es, cx                                ; 8e c1                       ; 0xc3335 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3337
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc333a vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc333d
    inc bx                                    ; 43                          ; 0xc333e vgabios.c:2221
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc333f vgabios.c:2222
    jmp short 03328h                          ; eb e4                       ; 0xc3342
    xor al, al                                ; 30 c0                       ; 0xc3344 vgabios.c:2223
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3346
    out DX, AL                                ; ee                          ; 0xc3349
    mov es, cx                                ; 8e c1                       ; 0xc334a vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc334c
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc334f vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3352
    inc bx                                    ; 43                          ; 0xc3353 vgabios.c:2224
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3354
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3357
    out DX, ax                                ; ef                          ; 0xc335a
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc335b vgabios.c:2229
    jmp short 0336bh                          ; eb 09                       ; 0xc3360
    jmp near 03442h                           ; e9 dd 00                    ; 0xc3362
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc3365
    jnbe short 03385h                         ; 77 1a                       ; 0xc3369
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc336b vgabios.c:2230
    je short 0337fh                           ; 74 0e                       ; 0xc336f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3371 vgabios.c:2231
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3374
    out DX, AL                                ; ee                          ; 0xc3377
    mov es, cx                                ; 8e c1                       ; 0xc3378 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc337a
    inc dx                                    ; 42                          ; 0xc337d vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc337e
    inc bx                                    ; 43                          ; 0xc337f vgabios.c:2234
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3380 vgabios.c:2235
    jmp short 03365h                          ; eb e0                       ; 0xc3383
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3385 vgabios.c:2237
    in AL, DX                                 ; ec                          ; 0xc3388
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3389
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc338b
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc338d
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc3390 vgabios.c:2238
    jne short 0339bh                          ; 75 04                       ; 0xc3395
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc3397 vgabios.c:2239
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc339b vgabios.c:2240
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc339e
    out DX, AL                                ; ee                          ; 0xc33a1
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc33a2 vgabios.c:2243
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc33a4
    out DX, AL                                ; ee                          ; 0xc33a7
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc33a8 vgabios.c:2244
    mov es, cx                                ; 8e c1                       ; 0xc33ac vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc33ae
    inc dx                                    ; 42                          ; 0xc33b1 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33b2
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc33b3 vgabios.c:2247
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc33b6 vgabios.c:37
    xor dh, dh                                ; 30 f6                       ; 0xc33b9 vgabios.c:38
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc33bb
    mov dx, 003dah                            ; ba da 03                    ; 0xc33be vgabios.c:2248
    in AL, DX                                 ; ec                          ; 0xc33c1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33c2
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33c4 vgabios.c:2249
    jmp short 033d1h                          ; eb 06                       ; 0xc33c9
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc33cb
    jnbe short 033eah                         ; 77 19                       ; 0xc33cf
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc33d1 vgabios.c:2250
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc33d4
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc33d7
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc33da
    out DX, AL                                ; ee                          ; 0xc33dd
    mov es, cx                                ; 8e c1                       ; 0xc33de vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33e0
    out DX, AL                                ; ee                          ; 0xc33e3 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc33e4 vgabios.c:2251
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33e5 vgabios.c:2252
    jmp short 033cbh                          ; eb e1                       ; 0xc33e8
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc33ea vgabios.c:2253
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc33ed
    out DX, AL                                ; ee                          ; 0xc33f0
    mov dx, 003dah                            ; ba da 03                    ; 0xc33f1 vgabios.c:2254
    in AL, DX                                 ; ec                          ; 0xc33f4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33f5
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33f7 vgabios.c:2256
    jmp short 03404h                          ; eb 06                       ; 0xc33fc
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc33fe
    jnbe short 0341ah                         ; 77 16                       ; 0xc3402
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3404 vgabios.c:2257
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3407
    out DX, AL                                ; ee                          ; 0xc340a
    mov es, cx                                ; 8e c1                       ; 0xc340b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc340d
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc3410 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3413
    inc bx                                    ; 43                          ; 0xc3414 vgabios.c:2258
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3415 vgabios.c:2259
    jmp short 033feh                          ; eb e4                       ; 0xc3418
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc341a vgabios.c:2260
    mov es, cx                                ; 8e c1                       ; 0xc341d vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc341f
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3422 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3425
    inc si                                    ; 46                          ; 0xc3426 vgabios.c:2263
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3427 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc342a vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc342d
    inc si                                    ; 46                          ; 0xc342e vgabios.c:2264
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc342f vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3432 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3435
    inc si                                    ; 46                          ; 0xc3436 vgabios.c:2265
    inc si                                    ; 46                          ; 0xc3437
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3438 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc343b vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc343e
    out DX, AL                                ; ee                          ; 0xc3441
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3442 vgabios.c:2269
    jne short 0344bh                          ; 75 03                       ; 0xc3446
    jmp near 03566h                           ; e9 1b 01                    ; 0xc3448
    mov es, cx                                ; 8e c1                       ; 0xc344b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc344d
    mov si, strict word 00049h                ; be 49 00                    ; 0xc3450 vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3453
    mov es, dx                                ; 8e c2                       ; 0xc3456
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3458
    inc bx                                    ; 43                          ; 0xc345b vgabios.c:2270
    mov es, cx                                ; 8e c1                       ; 0xc345c vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc345e
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc3461 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3464
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3466
    inc bx                                    ; 43                          ; 0xc3469 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc346a
    mov es, cx                                ; 8e c1                       ; 0xc346b vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc346d
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3470 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3473
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3475
    inc bx                                    ; 43                          ; 0xc3478 vgabios.c:2272
    inc bx                                    ; 43                          ; 0xc3479
    mov es, cx                                ; 8e c1                       ; 0xc347a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc347c
    mov si, strict word 00063h                ; be 63 00                    ; 0xc347f vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3482
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3484
    inc bx                                    ; 43                          ; 0xc3487 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc3488
    mov es, cx                                ; 8e c1                       ; 0xc3489 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc348b
    mov si, 00084h                            ; be 84 00                    ; 0xc348e vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3491
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3493
    inc bx                                    ; 43                          ; 0xc3496 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc3497 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3499
    mov si, 00085h                            ; be 85 00                    ; 0xc349c vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc349f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34a1
    inc bx                                    ; 43                          ; 0xc34a4 vgabios.c:2275
    inc bx                                    ; 43                          ; 0xc34a5
    mov es, cx                                ; 8e c1                       ; 0xc34a6 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34a8
    mov si, 00087h                            ; be 87 00                    ; 0xc34ab vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34ae
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34b0
    inc bx                                    ; 43                          ; 0xc34b3 vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc34b4 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34b6
    mov si, 00088h                            ; be 88 00                    ; 0xc34b9 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34bc
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34be
    inc bx                                    ; 43                          ; 0xc34c1 vgabios.c:2277
    mov es, cx                                ; 8e c1                       ; 0xc34c2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34c4
    mov si, 00089h                            ; be 89 00                    ; 0xc34c7 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34ca
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34cc
    inc bx                                    ; 43                          ; 0xc34cf vgabios.c:2278
    mov es, cx                                ; 8e c1                       ; 0xc34d0 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34d2
    mov si, strict word 00060h                ; be 60 00                    ; 0xc34d5 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34d8
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34da
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc34dd vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc34e2 vgabios.c:2279
    inc bx                                    ; 43                          ; 0xc34e3
    jmp short 034ech                          ; eb 06                       ; 0xc34e4
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc34e6
    jnc short 03508h                          ; 73 1c                       ; 0xc34ea
    mov es, cx                                ; 8e c1                       ; 0xc34ec vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34ee
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc34f1 vgabios.c:48
    add si, si                                ; 01 f6                       ; 0xc34f4
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc34f6
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc34f9 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34fc
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34fe
    inc bx                                    ; 43                          ; 0xc3501 vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc3502
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3503 vgabios.c:2283
    jmp short 034e6h                          ; eb de                       ; 0xc3506
    mov es, cx                                ; 8e c1                       ; 0xc3508 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc350a
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc350d vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3510
    mov es, dx                                ; 8e c2                       ; 0xc3513
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3515
    inc bx                                    ; 43                          ; 0xc3518 vgabios.c:2284
    inc bx                                    ; 43                          ; 0xc3519
    mov es, cx                                ; 8e c1                       ; 0xc351a vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc351c
    mov si, strict word 00062h                ; be 62 00                    ; 0xc351f vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3522
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3524
    inc bx                                    ; 43                          ; 0xc3527 vgabios.c:2285
    mov es, cx                                ; 8e c1                       ; 0xc3528 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc352a
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc352d vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc3530
    mov es, dx                                ; 8e c2                       ; 0xc3532
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3534
    inc bx                                    ; 43                          ; 0xc3537 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc3538
    mov es, cx                                ; 8e c1                       ; 0xc3539 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc353b
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc353e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3541
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3543
    inc bx                                    ; 43                          ; 0xc3546 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc3547
    mov es, cx                                ; 8e c1                       ; 0xc3548 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc354a
    mov si, 0010ch                            ; be 0c 01                    ; 0xc354d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3550
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3552
    inc bx                                    ; 43                          ; 0xc3555 vgabios.c:2289
    inc bx                                    ; 43                          ; 0xc3556
    mov es, cx                                ; 8e c1                       ; 0xc3557 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3559
    mov si, 0010eh                            ; be 0e 01                    ; 0xc355c vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc355f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3561
    inc bx                                    ; 43                          ; 0xc3564 vgabios.c:2290
    inc bx                                    ; 43                          ; 0xc3565
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3566 vgabios.c:2292
    je short 035a9h                           ; 74 3d                       ; 0xc356a
    inc bx                                    ; 43                          ; 0xc356c vgabios.c:2293
    mov es, cx                                ; 8e c1                       ; 0xc356d vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc356f
    xor ah, ah                                ; 30 e4                       ; 0xc3572 vgabios.c:38
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3574
    inc bx                                    ; 43                          ; 0xc3577 vgabios.c:2294
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3578 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc357b vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc357e
    inc bx                                    ; 43                          ; 0xc357f vgabios.c:2295
    xor al, al                                ; 30 c0                       ; 0xc3580
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3582
    out DX, AL                                ; ee                          ; 0xc3585
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3586 vgabios.c:2298
    jmp short 03592h                          ; eb 07                       ; 0xc3589
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc358b
    jnc short 035a1h                          ; 73 0f                       ; 0xc3590
    mov es, cx                                ; 8e c1                       ; 0xc3592 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3594
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3597 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc359a
    inc bx                                    ; 43                          ; 0xc359b vgabios.c:2299
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc359c vgabios.c:2300
    jmp short 0358bh                          ; eb ea                       ; 0xc359f
    inc bx                                    ; 43                          ; 0xc35a1 vgabios.c:2301
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc35a2
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35a5
    out DX, AL                                ; ee                          ; 0xc35a8
    mov ax, bx                                ; 89 d8                       ; 0xc35a9 vgabios.c:2305
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc35ab
    pop di                                    ; 5f                          ; 0xc35ae
    pop si                                    ; 5e                          ; 0xc35af
    pop cx                                    ; 59                          ; 0xc35b0
    pop bp                                    ; 5d                          ; 0xc35b1
    retn                                      ; c3                          ; 0xc35b2
  ; disGetNextSymbol 0xc35b3 LB 0xcbc -> off=0x0 cb=0000000000000028 uValue=00000000000c35b3 'find_vga_entry'
find_vga_entry:                              ; 0xc35b3 LB 0x28
    push bx                                   ; 53                          ; 0xc35b3 vgabios.c:2314
    push dx                                   ; 52                          ; 0xc35b4
    push bp                                   ; 55                          ; 0xc35b5
    mov bp, sp                                ; 89 e5                       ; 0xc35b6
    mov dl, al                                ; 88 c2                       ; 0xc35b8
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc35ba vgabios.c:2316
    xor al, al                                ; 30 c0                       ; 0xc35bc vgabios.c:2317
    jmp short 035c6h                          ; eb 06                       ; 0xc35be
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc35c0 vgabios.c:2318
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc35c2
    jnbe short 035d5h                         ; 77 0f                       ; 0xc35c4
    mov bl, al                                ; 88 c3                       ; 0xc35c6
    xor bh, bh                                ; 30 ff                       ; 0xc35c8
    sal bx, 003h                              ; c1 e3 03                    ; 0xc35ca
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc35cd
    jne short 035c0h                          ; 75 ed                       ; 0xc35d1
    mov ah, al                                ; 88 c4                       ; 0xc35d3
    mov al, ah                                ; 88 e0                       ; 0xc35d5 vgabios.c:2323
    pop bp                                    ; 5d                          ; 0xc35d7
    pop dx                                    ; 5a                          ; 0xc35d8
    pop bx                                    ; 5b                          ; 0xc35d9
    retn                                      ; c3                          ; 0xc35da
  ; disGetNextSymbol 0xc35db LB 0xc94 -> off=0x0 cb=000000000000000e uValue=00000000000c35db 'readx_byte'
readx_byte:                                  ; 0xc35db LB 0xe
    push bx                                   ; 53                          ; 0xc35db vgabios.c:2335
    push bp                                   ; 55                          ; 0xc35dc
    mov bp, sp                                ; 89 e5                       ; 0xc35dd
    mov bx, dx                                ; 89 d3                       ; 0xc35df
    mov es, ax                                ; 8e c0                       ; 0xc35e1 vgabios.c:2337
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35e3
    pop bp                                    ; 5d                          ; 0xc35e6 vgabios.c:2338
    pop bx                                    ; 5b                          ; 0xc35e7
    retn                                      ; c3                          ; 0xc35e8
  ; disGetNextSymbol 0xc35e9 LB 0xc86 -> off=0x87 cb=000000000000045c uValue=00000000000c3670 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0c5h, 03ah, 099h, 036h, 0d6h, 036h, 0ebh, 036h, 0fbh, 036h
    db  00eh, 037h, 01eh, 037h, 028h, 037h, 06ah, 037h, 0a4h, 037h, 0b5h, 037h, 0d2h, 037h, 0f1h, 037h
    db  017h, 038h, 034h, 038h, 04ah, 038h, 056h, 038h, 01eh, 039h, 088h, 039h, 0b5h, 039h, 0cah, 039h
    db  00ch, 03ah, 097h, 03ah, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 0c5h, 03ah, 075h, 038h, 096h, 038h, 0a5h, 038h, 0b4h, 038h, 075h, 038h, 096h, 038h
    db  0a5h, 038h, 0b4h, 038h, 0c3h, 038h, 0cfh, 038h, 0e8h, 038h, 0f2h, 038h, 0fch, 038h, 006h, 039h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 089h, 03ah, 032h, 03ah, 040h, 03ah, 051h, 03ah, 061h
    db  03ah, 076h, 03ah, 089h, 03ah, 089h, 03ah
int10_func:                                  ; 0xc3670 LB 0x45c
    push bp                                   ; 55                          ; 0xc3670 vgabios.c:2416
    mov bp, sp                                ; 89 e5                       ; 0xc3671
    push si                                   ; 56                          ; 0xc3673
    push di                                   ; 57                          ; 0xc3674
    push ax                                   ; 50                          ; 0xc3675
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3676
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3679 vgabios.c:2421
    shr ax, 008h                              ; c1 e8 08                    ; 0xc367c
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc367f
    jnbe short 036e8h                         ; 77 64                       ; 0xc3682
    push CS                                   ; 0e                          ; 0xc3684
    pop ES                                    ; 07                          ; 0xc3685
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3686
    mov di, 035e9h                            ; bf e9 35                    ; 0xc3689
    repne scasb                               ; f2 ae                       ; 0xc368c
    sal cx, 1                                 ; d1 e1                       ; 0xc368e
    mov di, cx                                ; 89 cf                       ; 0xc3690
    mov ax, word [cs:di+035ffh]               ; 2e 8b 85 ff 35              ; 0xc3692
    jmp ax                                    ; ff e0                       ; 0xc3697
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3699 vgabios.c:2424
    xor ah, ah                                ; 30 e4                       ; 0xc369c
    call 0137eh                               ; e8 dd dc                    ; 0xc369e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36a1 vgabios.c:2425
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc36a4
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc36a7
    je short 036c1h                           ; 74 15                       ; 0xc36aa
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc36ac
    je short 036b8h                           ; 74 07                       ; 0xc36af
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc36b1
    jbe short 036c1h                          ; 76 0b                       ; 0xc36b4
    jmp short 036cah                          ; eb 12                       ; 0xc36b6
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36b8 vgabios.c:2427
    xor al, al                                ; 30 c0                       ; 0xc36bb
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc36bd
    jmp short 036d1h                          ; eb 10                       ; 0xc36bf vgabios.c:2428
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36c1 vgabios.c:2436
    xor al, al                                ; 30 c0                       ; 0xc36c4
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc36c6
    jmp short 036d1h                          ; eb 07                       ; 0xc36c8
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36ca vgabios.c:2439
    xor al, al                                ; 30 c0                       ; 0xc36cd
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc36cf
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc36d1
    jmp short 036e8h                          ; eb 12                       ; 0xc36d4 vgabios.c:2441
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc36d6 vgabios.c:2443
    xor ah, ah                                ; 30 e4                       ; 0xc36d9
    mov dx, ax                                ; 89 c2                       ; 0xc36db
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc36dd
    shr ax, 008h                              ; c1 e8 08                    ; 0xc36e0
    xor ah, ah                                ; 30 e4                       ; 0xc36e3
    call 01110h                               ; e8 28 da                    ; 0xc36e5
    jmp near 03ac5h                           ; e9 da 03                    ; 0xc36e8 vgabios.c:2444
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc36eb vgabios.c:2446
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc36ee
    shr ax, 008h                              ; c1 e8 08                    ; 0xc36f1
    xor ah, ah                                ; 30 e4                       ; 0xc36f4
    call 01217h                               ; e8 1e db                    ; 0xc36f6
    jmp short 036e8h                          ; eb ed                       ; 0xc36f9 vgabios.c:2447
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc36fb vgabios.c:2449
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc36fe
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3701
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3704
    xor ah, ah                                ; 30 e4                       ; 0xc3707
    call 00a1ah                               ; e8 0e d3                    ; 0xc3709
    jmp short 036e8h                          ; eb da                       ; 0xc370c vgabios.c:2450
    xor ax, ax                                ; 31 c0                       ; 0xc370e vgabios.c:2456
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3710
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3713 vgabios.c:2457
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3716 vgabios.c:2458
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3719 vgabios.c:2459
    jmp short 036e8h                          ; eb ca                       ; 0xc371c vgabios.c:2460
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc371e vgabios.c:2462
    xor ah, ah                                ; 30 e4                       ; 0xc3721
    call 012a6h                               ; e8 80 db                    ; 0xc3723
    jmp short 036e8h                          ; eb c0                       ; 0xc3726 vgabios.c:2463
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3728 vgabios.c:2465
    push ax                                   ; 50                          ; 0xc372b
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc372c
    push ax                                   ; 50                          ; 0xc372f
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3730
    xor ah, ah                                ; 30 e4                       ; 0xc3733
    push ax                                   ; 50                          ; 0xc3735
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3736
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3739
    xor ah, ah                                ; 30 e4                       ; 0xc373c
    push ax                                   ; 50                          ; 0xc373e
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc373f
    mov cx, ax                                ; 89 c1                       ; 0xc3742
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3744
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3747
    mov bl, al                                ; 88 c3                       ; 0xc374a
    xor bh, bh                                ; 30 ff                       ; 0xc374c
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc374e
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3751
    xor ah, ah                                ; 30 e4                       ; 0xc3754
    mov dx, ax                                ; 89 c2                       ; 0xc3756
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3758
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc375b
    mov byte [bp-005h], bh                    ; 88 7e fb                    ; 0xc375e
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3761
    call 01a34h                               ; e8 cd e2                    ; 0xc3764
    jmp near 03ac5h                           ; e9 5b 03                    ; 0xc3767 vgabios.c:2466
    xor ax, ax                                ; 31 c0                       ; 0xc376a vgabios.c:2468
    push ax                                   ; 50                          ; 0xc376c
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc376d
    push ax                                   ; 50                          ; 0xc3770
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3771
    xor ah, ah                                ; 30 e4                       ; 0xc3774
    push ax                                   ; 50                          ; 0xc3776
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3777
    shr ax, 008h                              ; c1 e8 08                    ; 0xc377a
    xor ah, ah                                ; 30 e4                       ; 0xc377d
    push ax                                   ; 50                          ; 0xc377f
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3780
    mov cx, ax                                ; 89 c1                       ; 0xc3783
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3785
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3788
    xor ah, ah                                ; 30 e4                       ; 0xc378b
    mov bx, ax                                ; 89 c3                       ; 0xc378d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc378f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3792
    xor ah, ah                                ; 30 e4                       ; 0xc3795
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc3797
    xor dh, dh                                ; 30 f6                       ; 0xc379a
    mov si, dx                                ; 89 d6                       ; 0xc379c
    mov dx, ax                                ; 89 c2                       ; 0xc379e
    mov ax, si                                ; 89 f0                       ; 0xc37a0
    jmp short 03764h                          ; eb c0                       ; 0xc37a2
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc37a4 vgabios.c:2471
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37a7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37aa
    xor ah, ah                                ; 30 e4                       ; 0xc37ad
    call 00d5ah                               ; e8 a8 d5                    ; 0xc37af
    jmp near 03ac5h                           ; e9 10 03                    ; 0xc37b2 vgabios.c:2472
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc37b5 vgabios.c:2474
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc37b8
    xor ah, ah                                ; 30 e4                       ; 0xc37bb
    mov bx, ax                                ; 89 c3                       ; 0xc37bd
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37bf
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37c2
    xor ah, ah                                ; 30 e4                       ; 0xc37c5
    mov dx, ax                                ; 89 c2                       ; 0xc37c7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37c9
    call 0237ah                               ; e8 ab eb                    ; 0xc37cc
    jmp near 03ac5h                           ; e9 f3 02                    ; 0xc37cf vgabios.c:2475
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc37d2 vgabios.c:2477
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc37d5
    xor ah, ah                                ; 30 e4                       ; 0xc37d8
    mov bx, ax                                ; 89 c3                       ; 0xc37da
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37dc
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37df
    mov dl, al                                ; 88 c2                       ; 0xc37e2
    xor dh, dh                                ; 30 f6                       ; 0xc37e4
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37e6
    xor ah, ah                                ; 30 e4                       ; 0xc37e9
    call 02501h                               ; e8 13 ed                    ; 0xc37eb
    jmp near 03ac5h                           ; e9 d4 02                    ; 0xc37ee vgabios.c:2478
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc37f1 vgabios.c:2480
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc37f4
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc37f7
    xor dh, dh                                ; 30 f6                       ; 0xc37fa
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37fc
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37ff
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3802
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3805
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3808
    mov byte [bp-005h], dh                    ; 88 76 fb                    ; 0xc380b
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc380e
    call 02682h                               ; e8 6e ee                    ; 0xc3811
    jmp near 03ac5h                           ; e9 ae 02                    ; 0xc3814 vgabios.c:2481
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc3817 vgabios.c:2483
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc381a
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc381d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3820
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3823
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3826
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3829
    xor ah, ah                                ; 30 e4                       ; 0xc382c
    call 00f1dh                               ; e8 ec d6                    ; 0xc382e
    jmp near 03ac5h                           ; e9 91 02                    ; 0xc3831 vgabios.c:2484
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3834 vgabios.c:2492
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3837
    xor ah, ah                                ; 30 e4                       ; 0xc383a
    mov bx, ax                                ; 89 c3                       ; 0xc383c
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc383e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3841
    call 027f5h                               ; e8 ae ef                    ; 0xc3844
    jmp near 03ac5h                           ; e9 7b 02                    ; 0xc3847 vgabios.c:2493
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc384a vgabios.c:2496
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc384d
    call 01083h                               ; e8 30 d8                    ; 0xc3850
    jmp near 03ac5h                           ; e9 6f 02                    ; 0xc3853 vgabios.c:2497
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3856 vgabios.c:2499
    xor ah, ah                                ; 30 e4                       ; 0xc3859
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc385b
    jnbe short 038cch                         ; 77 6c                       ; 0xc385e
    push CS                                   ; 0e                          ; 0xc3860
    pop ES                                    ; 07                          ; 0xc3861
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc3862
    mov di, 0362dh                            ; bf 2d 36                    ; 0xc3865
    repne scasb                               ; f2 ae                       ; 0xc3868
    sal cx, 1                                 ; d1 e1                       ; 0xc386a
    mov di, cx                                ; 89 cf                       ; 0xc386c
    mov ax, word [cs:di+0363bh]               ; 2e 8b 85 3b 36              ; 0xc386e
    jmp ax                                    ; ff e0                       ; 0xc3873
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3875 vgabios.c:2503
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3878
    xor ah, ah                                ; 30 e4                       ; 0xc387b
    push ax                                   ; 50                          ; 0xc387d
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc387e
    push ax                                   ; 50                          ; 0xc3881
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3882
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3885
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3888
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc388b
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc388e
    call 02b66h                               ; e8 d2 f2                    ; 0xc3891
    jmp short 038cch                          ; eb 36                       ; 0xc3894 vgabios.c:2504
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3896 vgabios.c:2507
    xor dh, dh                                ; 30 f6                       ; 0xc3899
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc389b
    xor ah, ah                                ; 30 e4                       ; 0xc389e
    call 02be5h                               ; e8 42 f3                    ; 0xc38a0
    jmp short 038cch                          ; eb 27                       ; 0xc38a3 vgabios.c:2508
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38a5 vgabios.c:2511
    xor dh, dh                                ; 30 f6                       ; 0xc38a8
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38aa
    xor ah, ah                                ; 30 e4                       ; 0xc38ad
    call 02c52h                               ; e8 a0 f3                    ; 0xc38af
    jmp short 038cch                          ; eb 18                       ; 0xc38b2 vgabios.c:2512
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38b4 vgabios.c:2515
    xor dh, dh                                ; 30 f6                       ; 0xc38b7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38b9
    xor ah, ah                                ; 30 e4                       ; 0xc38bc
    call 02cc1h                               ; e8 00 f4                    ; 0xc38be
    jmp short 038cch                          ; eb 09                       ; 0xc38c1 vgabios.c:2516
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38c3 vgabios.c:2518
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc38c6
    call 02d30h                               ; e8 64 f4                    ; 0xc38c9
    jmp near 03ac5h                           ; e9 f6 01                    ; 0xc38cc vgabios.c:2519
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc38cf vgabios.c:2521
    xor ah, ah                                ; 30 e4                       ; 0xc38d2
    push ax                                   ; 50                          ; 0xc38d4
    mov cl, byte [bp+00ch]                    ; 8a 4e 0c                    ; 0xc38d5
    xor ch, ch                                ; 30 ed                       ; 0xc38d8
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc38da
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38dd
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc38e0
    call 02d35h                               ; e8 4f f4                    ; 0xc38e3
    jmp short 038cch                          ; eb e4                       ; 0xc38e6 vgabios.c:2522
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38e8 vgabios.c:2524
    xor ah, ah                                ; 30 e4                       ; 0xc38eb
    call 02d3ch                               ; e8 4c f4                    ; 0xc38ed
    jmp short 038cch                          ; eb da                       ; 0xc38f0 vgabios.c:2525
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38f2 vgabios.c:2527
    xor ah, ah                                ; 30 e4                       ; 0xc38f5
    call 02d41h                               ; e8 47 f4                    ; 0xc38f7
    jmp short 038cch                          ; eb d0                       ; 0xc38fa vgabios.c:2528
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38fc vgabios.c:2530
    xor ah, ah                                ; 30 e4                       ; 0xc38ff
    call 02d46h                               ; e8 42 f4                    ; 0xc3901
    jmp short 038cch                          ; eb c6                       ; 0xc3904 vgabios.c:2531
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc3906 vgabios.c:2533
    push ax                                   ; 50                          ; 0xc3909
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc390a
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc390d
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3910
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3913
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3916
    call 00e9ah                               ; e8 7e d5                    ; 0xc3919
    jmp short 038cch                          ; eb ae                       ; 0xc391c vgabios.c:2541
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc391e vgabios.c:2543
    xor ah, ah                                ; 30 e4                       ; 0xc3921
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc3923
    jc short 03936h                           ; 72 0e                       ; 0xc3926
    jbe short 03940h                          ; 76 16                       ; 0xc3928
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc392a
    je short 03980h                           ; 74 51                       ; 0xc392d
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc392f
    je short 03972h                           ; 74 3e                       ; 0xc3932
    jmp short 038cch                          ; eb 96                       ; 0xc3934
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc3936
    jne short 0396fh                          ; 75 34                       ; 0xc3939
    call 02d4bh                               ; e8 0d f4                    ; 0xc393b vgabios.c:2546
    jmp short 0396fh                          ; eb 2f                       ; 0xc393e vgabios.c:2547
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3940 vgabios.c:2549
    xor ah, ah                                ; 30 e4                       ; 0xc3943
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3945
    jnc short 0396ch                          ; 73 22                       ; 0xc3948
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc394a vgabios.c:35
    mov bx, 00087h                            ; bb 87 00                    ; 0xc394d
    mov es, ax                                ; 8e c0                       ; 0xc3950 vgabios.c:37
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc3952
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc3955 vgabios.c:38
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3958
    or dl, al                                 ; 08 c2                       ; 0xc395b
    mov si, bx                                ; 89 de                       ; 0xc395d vgabios.c:40
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc395f vgabios.c:42
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3962 vgabios.c:2552
    xor al, al                                ; 30 c0                       ; 0xc3965
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3967
    jmp near 036d1h                           ; e9 65 fd                    ; 0xc3969
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc396c vgabios.c:2555
    jmp near 03ac5h                           ; e9 53 01                    ; 0xc396f vgabios.c:2556
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3972 vgabios.c:2558
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3975
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3978
    call 02d50h                               ; e8 d2 f3                    ; 0xc397b
    jmp short 03962h                          ; eb e2                       ; 0xc397e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3980 vgabios.c:2562
    call 02d55h                               ; e8 cf f3                    ; 0xc3983
    jmp short 03962h                          ; eb da                       ; 0xc3986
    push word [bp+008h]                       ; ff 76 08                    ; 0xc3988 vgabios.c:2572
    push word [bp+016h]                       ; ff 76 16                    ; 0xc398b
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc398e
    xor ah, ah                                ; 30 e4                       ; 0xc3991
    push ax                                   ; 50                          ; 0xc3993
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3994
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3997
    xor ah, ah                                ; 30 e4                       ; 0xc399a
    push ax                                   ; 50                          ; 0xc399c
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc399d
    xor bh, bh                                ; 30 ff                       ; 0xc39a0
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc39a2
    shr dx, 008h                              ; c1 ea 08                    ; 0xc39a5
    xor dh, dh                                ; 30 f6                       ; 0xc39a8
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39aa
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc39ad
    call 02d5ah                               ; e8 a7 f3                    ; 0xc39b0
    jmp short 0396fh                          ; eb ba                       ; 0xc39b3 vgabios.c:2573
    mov bx, si                                ; 89 f3                       ; 0xc39b5 vgabios.c:2575
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39b7
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39ba
    call 02df7h                               ; e8 37 f4                    ; 0xc39bd
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39c0 vgabios.c:2576
    xor al, al                                ; 30 c0                       ; 0xc39c3
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc39c5
    jmp near 036d1h                           ; e9 07 fd                    ; 0xc39c7
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39ca vgabios.c:2579
    xor ah, ah                                ; 30 e4                       ; 0xc39cd
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc39cf
    je short 039f6h                           ; 74 22                       ; 0xc39d2
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc39d4
    je short 039e8h                           ; 74 0f                       ; 0xc39d7
    test ax, ax                               ; 85 c0                       ; 0xc39d9
    jne short 03a02h                          ; 75 25                       ; 0xc39db
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc39dd vgabios.c:2582
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39e0
    call 03009h                               ; e8 23 f6                    ; 0xc39e3
    jmp short 03a02h                          ; eb 1a                       ; 0xc39e6 vgabios.c:2583
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc39e8 vgabios.c:2585
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39eb
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39ee
    call 03021h                               ; e8 2d f6                    ; 0xc39f1
    jmp short 03a02h                          ; eb 0c                       ; 0xc39f4 vgabios.c:2586
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc39f6 vgabios.c:2588
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39f9
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39fc
    call 032f9h                               ; e8 f7 f8                    ; 0xc39ff
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a02 vgabios.c:2595
    xor al, al                                ; 30 c0                       ; 0xc3a05
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3a07
    jmp near 036d1h                           ; e9 c5 fc                    ; 0xc3a09
    call 007afh                               ; e8 a0 cd                    ; 0xc3a0c vgabios.c:2600
    test ax, ax                               ; 85 c0                       ; 0xc3a0f
    je short 03a87h                           ; 74 74                       ; 0xc3a11
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a13 vgabios.c:2601
    xor ah, ah                                ; 30 e4                       ; 0xc3a16
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3a18
    jnbe short 03a89h                         ; 77 6c                       ; 0xc3a1b
    push CS                                   ; 0e                          ; 0xc3a1d
    pop ES                                    ; 07                          ; 0xc3a1e
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3a1f
    mov di, 03659h                            ; bf 59 36                    ; 0xc3a22
    repne scasb                               ; f2 ae                       ; 0xc3a25
    sal cx, 1                                 ; d1 e1                       ; 0xc3a27
    mov di, cx                                ; 89 cf                       ; 0xc3a29
    mov ax, word [cs:di+03660h]               ; 2e 8b 85 60 36              ; 0xc3a2b
    jmp ax                                    ; ff e0                       ; 0xc3a30
    mov bx, si                                ; 89 f3                       ; 0xc3a32 vgabios.c:2604
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a34
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a37
    call 03c7fh                               ; e8 42 02                    ; 0xc3a3a
    jmp near 03ac5h                           ; e9 85 00                    ; 0xc3a3d vgabios.c:2605
    mov cx, si                                ; 89 f1                       ; 0xc3a40 vgabios.c:2607
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a42
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a45
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a48
    call 03daah                               ; e8 5c 03                    ; 0xc3a4b
    jmp near 03ac5h                           ; e9 74 00                    ; 0xc3a4e vgabios.c:2608
    mov cx, si                                ; 89 f1                       ; 0xc3a51 vgabios.c:2610
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a53
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3a56
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a59
    call 03e49h                               ; e8 ea 03                    ; 0xc3a5c
    jmp short 03ac5h                          ; eb 64                       ; 0xc3a5f vgabios.c:2611
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3a61 vgabios.c:2613
    push ax                                   ; 50                          ; 0xc3a64
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3a65
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3a68
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a6b
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a6e
    call 04012h                               ; e8 9e 05                    ; 0xc3a71
    jmp short 03ac5h                          ; eb 4f                       ; 0xc3a74 vgabios.c:2614
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3a76 vgabios.c:2616
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3a79
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3a7c
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a7f
    call 0409eh                               ; e8 19 06                    ; 0xc3a82
    jmp short 03ac5h                          ; eb 3e                       ; 0xc3a85 vgabios.c:2617
    jmp short 03a90h                          ; eb 07                       ; 0xc3a87
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3a89 vgabios.c:2639
    jmp short 03ac5h                          ; eb 35                       ; 0xc3a8e vgabios.c:2642
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3a90 vgabios.c:2644
    jmp short 03ac5h                          ; eb 2e                       ; 0xc3a95 vgabios.c:2646
    call 007afh                               ; e8 15 cd                    ; 0xc3a97 vgabios.c:2648
    test ax, ax                               ; 85 c0                       ; 0xc3a9a
    je short 03ac0h                           ; 74 22                       ; 0xc3a9c
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a9e vgabios.c:2649
    xor ah, ah                                ; 30 e4                       ; 0xc3aa1
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3aa3
    jne short 03ab9h                          ; 75 11                       ; 0xc3aa6
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3aa8 vgabios.c:2652
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3aab
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3aae
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3ab1
    call 0417dh                               ; e8 c6 06                    ; 0xc3ab4
    jmp short 03ac5h                          ; eb 0c                       ; 0xc3ab7 vgabios.c:2653
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ab9 vgabios.c:2655
    jmp short 03ac5h                          ; eb 05                       ; 0xc3abe vgabios.c:2658
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ac0 vgabios.c:2660
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ac5 vgabios.c:2670
    pop di                                    ; 5f                          ; 0xc3ac8
    pop si                                    ; 5e                          ; 0xc3ac9
    pop bp                                    ; 5d                          ; 0xc3aca
    retn                                      ; c3                          ; 0xc3acb
  ; disGetNextSymbol 0xc3acc LB 0x7a3 -> off=0x0 cb=000000000000001f uValue=00000000000c3acc 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3acc LB 0x1f
    push bp                                   ; 55                          ; 0xc3acc vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3acd
    push bx                                   ; 53                          ; 0xc3acf
    push dx                                   ; 52                          ; 0xc3ad0
    mov bx, ax                                ; 89 c3                       ; 0xc3ad1
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3ad3 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ad6
    call 00560h                               ; e8 84 ca                    ; 0xc3ad9
    mov ax, bx                                ; 89 d8                       ; 0xc3adc vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ade
    call 00560h                               ; e8 7c ca                    ; 0xc3ae1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ae4 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3ae7
    pop bx                                    ; 5b                          ; 0xc3ae8
    pop bp                                    ; 5d                          ; 0xc3ae9
    retn                                      ; c3                          ; 0xc3aea
  ; disGetNextSymbol 0xc3aeb LB 0x784 -> off=0x0 cb=000000000000001f uValue=00000000000c3aeb 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3aeb LB 0x1f
    push bp                                   ; 55                          ; 0xc3aeb vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3aec
    push bx                                   ; 53                          ; 0xc3aee
    push dx                                   ; 52                          ; 0xc3aef
    mov bx, ax                                ; 89 c3                       ; 0xc3af0
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3af2 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3af5
    call 00560h                               ; e8 65 ca                    ; 0xc3af8
    mov ax, bx                                ; 89 d8                       ; 0xc3afb vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3afd
    call 00560h                               ; e8 5d ca                    ; 0xc3b00
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b03 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3b06
    pop bx                                    ; 5b                          ; 0xc3b07
    pop bp                                    ; 5d                          ; 0xc3b08
    retn                                      ; c3                          ; 0xc3b09
  ; disGetNextSymbol 0xc3b0a LB 0x765 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b0a 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3b0a LB 0x19
    push bp                                   ; 55                          ; 0xc3b0a vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3b0b
    push dx                                   ; 52                          ; 0xc3b0d
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b0e vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b11
    call 00560h                               ; e8 49 ca                    ; 0xc3b14
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b17 vbe.c:121
    call 00567h                               ; e8 4a ca                    ; 0xc3b1a
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b1d vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3b20
    pop bp                                    ; 5d                          ; 0xc3b21
    retn                                      ; c3                          ; 0xc3b22
  ; disGetNextSymbol 0xc3b23 LB 0x74c -> off=0x0 cb=000000000000001f uValue=00000000000c3b23 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3b23 LB 0x1f
    push bp                                   ; 55                          ; 0xc3b23 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3b24
    push bx                                   ; 53                          ; 0xc3b26
    push dx                                   ; 52                          ; 0xc3b27
    mov bx, ax                                ; 89 c3                       ; 0xc3b28
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b2a vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b2d
    call 00560h                               ; e8 2d ca                    ; 0xc3b30
    mov ax, bx                                ; 89 d8                       ; 0xc3b33 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b35
    call 00560h                               ; e8 25 ca                    ; 0xc3b38
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b3b vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3b3e
    pop bx                                    ; 5b                          ; 0xc3b3f
    pop bp                                    ; 5d                          ; 0xc3b40
    retn                                      ; c3                          ; 0xc3b41
  ; disGetNextSymbol 0xc3b42 LB 0x72d -> off=0x0 cb=0000000000000019 uValue=00000000000c3b42 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3b42 LB 0x19
    push bp                                   ; 55                          ; 0xc3b42 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3b43
    push dx                                   ; 52                          ; 0xc3b45
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b46 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b49
    call 00560h                               ; e8 11 ca                    ; 0xc3b4c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b4f vbe.c:136
    call 00567h                               ; e8 12 ca                    ; 0xc3b52
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b55 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3b58
    pop bp                                    ; 5d                          ; 0xc3b59
    retn                                      ; c3                          ; 0xc3b5a
  ; disGetNextSymbol 0xc3b5b LB 0x714 -> off=0x0 cb=000000000000001f uValue=00000000000c3b5b 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3b5b LB 0x1f
    push bp                                   ; 55                          ; 0xc3b5b vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3b5c
    push bx                                   ; 53                          ; 0xc3b5e
    push dx                                   ; 52                          ; 0xc3b5f
    mov bx, ax                                ; 89 c3                       ; 0xc3b60
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b62 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b65
    call 00560h                               ; e8 f5 c9                    ; 0xc3b68
    mov ax, bx                                ; 89 d8                       ; 0xc3b6b vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b6d
    call 00560h                               ; e8 ed c9                    ; 0xc3b70
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b73 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3b76
    pop bx                                    ; 5b                          ; 0xc3b77
    pop bp                                    ; 5d                          ; 0xc3b78
    retn                                      ; c3                          ; 0xc3b79
  ; disGetNextSymbol 0xc3b7a LB 0x6f5 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b7a 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3b7a LB 0x19
    push bp                                   ; 55                          ; 0xc3b7a vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3b7b
    push dx                                   ; 52                          ; 0xc3b7d
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b7e vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b81
    call 00560h                               ; e8 d9 c9                    ; 0xc3b84
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b87 vbe.c:151
    call 00567h                               ; e8 da c9                    ; 0xc3b8a
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b8d vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3b90
    pop bp                                    ; 5d                          ; 0xc3b91
    retn                                      ; c3                          ; 0xc3b92
  ; disGetNextSymbol 0xc3b93 LB 0x6dc -> off=0x0 cb=0000000000000019 uValue=00000000000c3b93 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3b93 LB 0x19
    push bp                                   ; 55                          ; 0xc3b93 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3b94
    push dx                                   ; 52                          ; 0xc3b96
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3b97 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b9a
    call 00560h                               ; e8 c0 c9                    ; 0xc3b9d
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ba0 vbe.c:157
    call 00567h                               ; e8 c1 c9                    ; 0xc3ba3
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ba6 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3ba9
    pop bp                                    ; 5d                          ; 0xc3baa
    retn                                      ; c3                          ; 0xc3bab
  ; disGetNextSymbol 0xc3bac LB 0x6c3 -> off=0x0 cb=0000000000000012 uValue=00000000000c3bac 'in_word'
in_word:                                     ; 0xc3bac LB 0x12
    push bp                                   ; 55                          ; 0xc3bac vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3bad
    push bx                                   ; 53                          ; 0xc3baf
    mov bx, ax                                ; 89 c3                       ; 0xc3bb0
    mov ax, dx                                ; 89 d0                       ; 0xc3bb2
    mov dx, bx                                ; 89 da                       ; 0xc3bb4 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3bb6
    in ax, DX                                 ; ed                          ; 0xc3bb7 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bb8 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3bbb
    pop bp                                    ; 5d                          ; 0xc3bbc
    retn                                      ; c3                          ; 0xc3bbd
  ; disGetNextSymbol 0xc3bbe LB 0x6b1 -> off=0x0 cb=0000000000000014 uValue=00000000000c3bbe 'in_byte'
in_byte:                                     ; 0xc3bbe LB 0x14
    push bp                                   ; 55                          ; 0xc3bbe vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3bbf
    push bx                                   ; 53                          ; 0xc3bc1
    mov bx, ax                                ; 89 c3                       ; 0xc3bc2
    mov ax, dx                                ; 89 d0                       ; 0xc3bc4
    mov dx, bx                                ; 89 da                       ; 0xc3bc6 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3bc8
    in AL, DX                                 ; ec                          ; 0xc3bc9 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3bca
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bcc vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3bcf
    pop bp                                    ; 5d                          ; 0xc3bd0
    retn                                      ; c3                          ; 0xc3bd1
  ; disGetNextSymbol 0xc3bd2 LB 0x69d -> off=0x0 cb=0000000000000014 uValue=00000000000c3bd2 'dispi_get_id'
dispi_get_id:                                ; 0xc3bd2 LB 0x14
    push bp                                   ; 55                          ; 0xc3bd2 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3bd3
    push dx                                   ; 52                          ; 0xc3bd5
    xor ax, ax                                ; 31 c0                       ; 0xc3bd6 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bd8
    out DX, ax                                ; ef                          ; 0xc3bdb
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bdc vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3bdf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3be0 vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3be3
    pop bp                                    ; 5d                          ; 0xc3be4
    retn                                      ; c3                          ; 0xc3be5
  ; disGetNextSymbol 0xc3be6 LB 0x689 -> off=0x0 cb=000000000000001a uValue=00000000000c3be6 'dispi_set_id'
dispi_set_id:                                ; 0xc3be6 LB 0x1a
    push bp                                   ; 55                          ; 0xc3be6 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3be7
    push bx                                   ; 53                          ; 0xc3be9
    push dx                                   ; 52                          ; 0xc3bea
    mov bx, ax                                ; 89 c3                       ; 0xc3beb
    xor ax, ax                                ; 31 c0                       ; 0xc3bed vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bef
    out DX, ax                                ; ef                          ; 0xc3bf2
    mov ax, bx                                ; 89 d8                       ; 0xc3bf3 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bf5
    out DX, ax                                ; ef                          ; 0xc3bf8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3bf9 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3bfc
    pop bx                                    ; 5b                          ; 0xc3bfd
    pop bp                                    ; 5d                          ; 0xc3bfe
    retn                                      ; c3                          ; 0xc3bff
  ; disGetNextSymbol 0xc3c00 LB 0x66f -> off=0x0 cb=000000000000002a uValue=00000000000c3c00 'vbe_init'
vbe_init:                                    ; 0xc3c00 LB 0x2a
    push bp                                   ; 55                          ; 0xc3c00 vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3c01
    push bx                                   ; 53                          ; 0xc3c03
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3c04 vbe.c:190
    call 03be6h                               ; e8 dc ff                    ; 0xc3c07
    call 03bd2h                               ; e8 c5 ff                    ; 0xc3c0a vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3c0d
    jne short 03c24h                          ; 75 12                       ; 0xc3c10
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3c12 vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3c15
    mov es, ax                                ; 8e c0                       ; 0xc3c18
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3c1a
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3c1e vbe.c:194
    call 03be6h                               ; e8 c2 ff                    ; 0xc3c21
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c24 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3c27
    pop bp                                    ; 5d                          ; 0xc3c28
    retn                                      ; c3                          ; 0xc3c29
  ; disGetNextSymbol 0xc3c2a LB 0x645 -> off=0x0 cb=0000000000000055 uValue=00000000000c3c2a 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3c2a LB 0x55
    push bp                                   ; 55                          ; 0xc3c2a vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3c2b
    push bx                                   ; 53                          ; 0xc3c2d
    push cx                                   ; 51                          ; 0xc3c2e
    push si                                   ; 56                          ; 0xc3c2f
    push di                                   ; 57                          ; 0xc3c30
    mov di, ax                                ; 89 c7                       ; 0xc3c31
    mov si, dx                                ; 89 d6                       ; 0xc3c33
    xor dx, dx                                ; 31 d2                       ; 0xc3c35 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c37
    call 03bach                               ; e8 6f ff                    ; 0xc3c3a
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3c3d vbe.c:209
    jne short 03c74h                          ; 75 32                       ; 0xc3c40
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3c42 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3c45 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c47
    call 03bach                               ; e8 5f ff                    ; 0xc3c4a
    mov cx, ax                                ; 89 c1                       ; 0xc3c4d
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3c4f vbe.c:219
    je short 03c74h                           ; 74 20                       ; 0xc3c52
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3c54 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c57
    call 03bach                               ; e8 4f ff                    ; 0xc3c5a
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3c5d
    cmp cx, di                                ; 39 f9                       ; 0xc3c60 vbe.c:223
    jne short 03c70h                          ; 75 0c                       ; 0xc3c62
    test si, si                               ; 85 f6                       ; 0xc3c64 vbe.c:225
    jne short 03c6ch                          ; 75 04                       ; 0xc3c66
    mov ax, bx                                ; 89 d8                       ; 0xc3c68 vbe.c:226
    jmp short 03c76h                          ; eb 0a                       ; 0xc3c6a
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3c6c vbe.c:227
    jne short 03c68h                          ; 75 f8                       ; 0xc3c6e
    mov bx, dx                                ; 89 d3                       ; 0xc3c70 vbe.c:230
    jmp short 03c47h                          ; eb d3                       ; 0xc3c72 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3c74 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3c76 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3c79
    pop si                                    ; 5e                          ; 0xc3c7a
    pop cx                                    ; 59                          ; 0xc3c7b
    pop bx                                    ; 5b                          ; 0xc3c7c
    pop bp                                    ; 5d                          ; 0xc3c7d
    retn                                      ; c3                          ; 0xc3c7e
  ; disGetNextSymbol 0xc3c7f LB 0x5f0 -> off=0x0 cb=000000000000012b uValue=00000000000c3c7f 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3c7f LB 0x12b
    push bp                                   ; 55                          ; 0xc3c7f vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3c80
    push cx                                   ; 51                          ; 0xc3c82
    push si                                   ; 56                          ; 0xc3c83
    push di                                   ; 57                          ; 0xc3c84
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3c85
    mov si, ax                                ; 89 c6                       ; 0xc3c88
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3c8a
    mov di, bx                                ; 89 df                       ; 0xc3c8d
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3c8f vbe.c:275
    call 005a7h                               ; e8 10 c9                    ; 0xc3c94 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3c97
    mov bx, di                                ; 89 fb                       ; 0xc3c9a vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3c9c
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3c9f
    xor dx, dx                                ; 31 d2                       ; 0xc3ca2 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ca4
    call 03bach                               ; e8 02 ff                    ; 0xc3ca7
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3caa vbe.c:285
    je short 03cb9h                           ; 74 0a                       ; 0xc3cad
    push SS                                   ; 16                          ; 0xc3caf vbe.c:287
    pop ES                                    ; 07                          ; 0xc3cb0
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3cb1
    jmp near 03da2h                           ; e9 e9 00                    ; 0xc3cb6 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3cb9 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3cbc vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3cc1 vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3cc4
    jne short 03cd3h                          ; 75 07                       ; 0xc3cca
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3ccc
    je short 03ce2h                           ; 74 0f                       ; 0xc3cd1
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3cd3
    jne short 03ce7h                          ; 75 0c                       ; 0xc3cd9
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3cdb
    jne short 03ce7h                          ; 75 05                       ; 0xc3ce0
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3ce2 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3ce7 vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3cea
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc3cef vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3cf5 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3cfb vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3d01
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc3d05 vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3d0b vbe.c:332
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3d11 vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3d14
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3d18 vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3d1b
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3d1f vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d22
    call 03bach                               ; e8 84 fe                    ; 0xc3d25
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d28
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3d2b
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3d2f vbe.c:342
    je short 03d59h                           ; 74 24                       ; 0xc3d33
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3d35 vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3d3b vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3d41
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3d45 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3d4b
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3d4f vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3d55
    mov dx, cx                                ; 89 ca                       ; 0xc3d59 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3d5b
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d5e
    call 03bbeh                               ; e8 5a fe                    ; 0xc3d61
    xor ah, ah                                ; 30 e4                       ; 0xc3d64 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3d66
    jnbe short 03d82h                         ; 77 17                       ; 0xc3d69
    mov dx, cx                                ; 89 ca                       ; 0xc3d6b vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d6d
    call 03bach                               ; e8 39 fe                    ; 0xc3d70
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3d73 vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3d76
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3d78 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d7b
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3d7e vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3d82 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3d85 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d87
    call 03bach                               ; e8 1f fe                    ; 0xc3d8a
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3d8d vbe.c:368
    jne short 03d59h                          ; 75 c7                       ; 0xc3d90
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3d92 vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3d95 vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3d98
    push SS                                   ; 16                          ; 0xc3d9b vbe.c:372
    pop ES                                    ; 07                          ; 0xc3d9c
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3d9d
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3da2 vbe.c:373
    pop di                                    ; 5f                          ; 0xc3da5
    pop si                                    ; 5e                          ; 0xc3da6
    pop cx                                    ; 59                          ; 0xc3da7
    pop bp                                    ; 5d                          ; 0xc3da8
    retn                                      ; c3                          ; 0xc3da9
  ; disGetNextSymbol 0xc3daa LB 0x4c5 -> off=0x0 cb=000000000000009f uValue=00000000000c3daa 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3daa LB 0x9f
    push bp                                   ; 55                          ; 0xc3daa vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3dab
    push si                                   ; 56                          ; 0xc3dad
    push di                                   ; 57                          ; 0xc3dae
    push ax                                   ; 50                          ; 0xc3daf
    push ax                                   ; 50                          ; 0xc3db0
    mov ax, dx                                ; 89 d0                       ; 0xc3db1
    mov si, bx                                ; 89 de                       ; 0xc3db3
    mov bx, cx                                ; 89 cb                       ; 0xc3db5
    test dh, 040h                             ; f6 c6 40                    ; 0xc3db7 vbe.c:396
    je short 03dc1h                           ; 74 05                       ; 0xc3dba
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc3dbc
    jmp short 03dc3h                          ; eb 02                       ; 0xc3dbf
    xor dx, dx                                ; 31 d2                       ; 0xc3dc1
    and ah, 001h                              ; 80 e4 01                    ; 0xc3dc3 vbe.c:397
    call 03c2ah                               ; e8 61 fe                    ; 0xc3dc6 vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3dc9
    test ax, ax                               ; 85 c0                       ; 0xc3dcc vbe.c:401
    je short 03e37h                           ; 74 67                       ; 0xc3dce
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3dd0 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3dd3
    mov di, bx                                ; 89 df                       ; 0xc3dd5
    mov es, si                                ; 8e c6                       ; 0xc3dd7
    jcxz 03dddh                               ; e3 02                       ; 0xc3dd9
    rep stosb                                 ; f3 aa                       ; 0xc3ddb
    xor cx, cx                                ; 31 c9                       ; 0xc3ddd vbe.c:407
    jmp short 03de6h                          ; eb 05                       ; 0xc3ddf
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3de1
    jnc short 03dffh                          ; 73 19                       ; 0xc3de4
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3de6 vbe.c:410
    inc dx                                    ; 42                          ; 0xc3de9
    inc dx                                    ; 42                          ; 0xc3dea
    add dx, cx                                ; 01 ca                       ; 0xc3deb
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ded
    call 03bbeh                               ; e8 cb fd                    ; 0xc3df0
    mov di, bx                                ; 89 df                       ; 0xc3df3 vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3df5
    mov es, si                                ; 8e c6                       ; 0xc3df7 vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3df9
    inc cx                                    ; 41                          ; 0xc3dfc vbe.c:412
    jmp short 03de1h                          ; eb e2                       ; 0xc3dfd
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3dff vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3e02 vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3e04
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3e07 vbe.c:414
    je short 03e1bh                           ; 74 10                       ; 0xc3e09
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3e0b vbe.c:415
    mov word [es:di], 00619h                  ; 26 c7 05 19 06              ; 0xc3e0e vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3e13 vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3e16 vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3e1b vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e1e
    call 00560h                               ; e8 3c c7                    ; 0xc3e21
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e24 vbe.c:421
    call 00567h                               ; e8 3d c7                    ; 0xc3e27
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3e2a
    mov es, si                                ; 8e c6                       ; 0xc3e2d vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e2f
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3e32 vbe.c:423
    jmp short 03e3ah                          ; eb 03                       ; 0xc3e35 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3e37 vbe.c:428
    push SS                                   ; 16                          ; 0xc3e3a vbe.c:431
    pop ES                                    ; 07                          ; 0xc3e3b
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3e3c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e3f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e42 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3e45
    pop si                                    ; 5e                          ; 0xc3e46
    pop bp                                    ; 5d                          ; 0xc3e47
    retn                                      ; c3                          ; 0xc3e48
  ; disGetNextSymbol 0xc3e49 LB 0x426 -> off=0x0 cb=00000000000000e7 uValue=00000000000c3e49 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3e49 LB 0xe7
    push bp                                   ; 55                          ; 0xc3e49 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3e4a
    push si                                   ; 56                          ; 0xc3e4c
    push di                                   ; 57                          ; 0xc3e4d
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3e4e
    mov si, ax                                ; 89 c6                       ; 0xc3e51
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3e53
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3e56 vbe.c:452
    je short 03e61h                           ; 74 05                       ; 0xc3e5a
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e5c
    jmp short 03e63h                          ; eb 02                       ; 0xc3e5f
    xor ax, ax                                ; 31 c0                       ; 0xc3e61
    mov dx, ax                                ; 89 c2                       ; 0xc3e63
    test ax, ax                               ; 85 c0                       ; 0xc3e65 vbe.c:453
    je short 03e6ch                           ; 74 03                       ; 0xc3e67
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3e69
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3e6c
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3e6f vbe.c:454
    je short 03e7ah                           ; 74 05                       ; 0xc3e73
    mov ax, 00080h                            ; b8 80 00                    ; 0xc3e75
    jmp short 03e7ch                          ; eb 02                       ; 0xc3e78
    xor ax, ax                                ; 31 c0                       ; 0xc3e7a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3e7c
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3e7f vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3e83 vbe.c:459
    jnc short 03e9dh                          ; 73 13                       ; 0xc3e88
    xor ax, ax                                ; 31 c0                       ; 0xc3e8a vbe.c:463
    call 005cdh                               ; e8 3e c7                    ; 0xc3e8c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3e8f vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc3e92
    call 0137eh                               ; e8 e7 d4                    ; 0xc3e94
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3e97 vbe.c:468
    jmp near 03f24h                           ; e9 87 00                    ; 0xc3e9a vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3e9d vbe.c:472
    call 03c2ah                               ; e8 87 fd                    ; 0xc3ea0
    mov bx, ax                                ; 89 c3                       ; 0xc3ea3
    test ax, ax                               ; 85 c0                       ; 0xc3ea5 vbe.c:474
    je short 03f21h                           ; 74 78                       ; 0xc3ea7
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3ea9 vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3eac
    call 03bach                               ; e8 fa fc                    ; 0xc3eaf
    mov cx, ax                                ; 89 c1                       ; 0xc3eb2
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3eb4 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3eb7
    call 03bach                               ; e8 ef fc                    ; 0xc3eba
    mov di, ax                                ; 89 c7                       ; 0xc3ebd
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3ebf vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ec2
    call 03bbeh                               ; e8 f6 fc                    ; 0xc3ec5
    mov bl, al                                ; 88 c3                       ; 0xc3ec8
    mov dl, al                                ; 88 c2                       ; 0xc3eca
    xor ax, ax                                ; 31 c0                       ; 0xc3ecc vbe.c:489
    call 005cdh                               ; e8 fc c6                    ; 0xc3ece
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3ed1 vbe.c:491
    jne short 03edch                          ; 75 06                       ; 0xc3ed4
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3ed6 vbe.c:493
    call 0137eh                               ; e8 a2 d4                    ; 0xc3ed9
    mov al, dl                                ; 88 d0                       ; 0xc3edc vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3ede
    call 03b23h                               ; e8 40 fc                    ; 0xc3ee0
    mov ax, cx                                ; 89 c8                       ; 0xc3ee3 vbe.c:497
    call 03acch                               ; e8 e4 fb                    ; 0xc3ee5
    mov ax, di                                ; 89 f8                       ; 0xc3ee8 vbe.c:498
    call 03aebh                               ; e8 fe fb                    ; 0xc3eea
    xor ax, ax                                ; 31 c0                       ; 0xc3eed vbe.c:499
    call 005f3h                               ; e8 01 c7                    ; 0xc3eef
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3ef2 vbe.c:500
    or dl, 001h                               ; 80 ca 01                    ; 0xc3ef5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3ef8
    xor ah, ah                                ; 30 e4                       ; 0xc3efb
    or al, dl                                 ; 08 d0                       ; 0xc3efd
    call 005cdh                               ; e8 cb c6                    ; 0xc3eff
    call 006c2h                               ; e8 bd c7                    ; 0xc3f02 vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3f05 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f08
    mov es, ax                                ; 8e c0                       ; 0xc3f0b
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3f0d
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f10
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3f13 vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3f16
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3f18 vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3f1b
    jmp near 03e97h                           ; e9 76 ff                    ; 0xc3f1e
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3f21 vbe.c:513
    push SS                                   ; 16                          ; 0xc3f24 vbe.c:517
    pop ES                                    ; 07                          ; 0xc3f25
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3f26
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f29 vbe.c:518
    pop di                                    ; 5f                          ; 0xc3f2c
    pop si                                    ; 5e                          ; 0xc3f2d
    pop bp                                    ; 5d                          ; 0xc3f2e
    retn                                      ; c3                          ; 0xc3f2f
  ; disGetNextSymbol 0xc3f30 LB 0x33f -> off=0x0 cb=0000000000000008 uValue=00000000000c3f30 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3f30 LB 0x8
    push bp                                   ; 55                          ; 0xc3f30 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3f31
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3f33 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3f36
    retn                                      ; c3                          ; 0xc3f37
  ; disGetNextSymbol 0xc3f38 LB 0x337 -> off=0x0 cb=000000000000004b uValue=00000000000c3f38 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3f38 LB 0x4b
    push bp                                   ; 55                          ; 0xc3f38 vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3f39
    push bx                                   ; 53                          ; 0xc3f3b
    push cx                                   ; 51                          ; 0xc3f3c
    push si                                   ; 56                          ; 0xc3f3d
    mov si, ax                                ; 89 c6                       ; 0xc3f3e
    mov bx, dx                                ; 89 d3                       ; 0xc3f40
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3f42 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f45
    out DX, ax                                ; ef                          ; 0xc3f48
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f49 vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3f4c
    mov es, si                                ; 8e c6                       ; 0xc3f4d vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f4f
    inc bx                                    ; 43                          ; 0xc3f52 vbe.c:532
    inc bx                                    ; 43                          ; 0xc3f53
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3f54 vbe.c:533
    je short 03f7bh                           ; 74 23                       ; 0xc3f56
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3f58 vbe.c:535
    jmp short 03f62h                          ; eb 05                       ; 0xc3f5b
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3f5d
    jnbe short 03f7bh                         ; 77 19                       ; 0xc3f60
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3f62 vbe.c:536
    je short 03f78h                           ; 74 11                       ; 0xc3f65
    mov ax, cx                                ; 89 c8                       ; 0xc3f67 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f69
    out DX, ax                                ; ef                          ; 0xc3f6c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f6d vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3f70
    mov es, si                                ; 8e c6                       ; 0xc3f71 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f73
    inc bx                                    ; 43                          ; 0xc3f76 vbe.c:539
    inc bx                                    ; 43                          ; 0xc3f77
    inc cx                                    ; 41                          ; 0xc3f78 vbe.c:541
    jmp short 03f5dh                          ; eb e2                       ; 0xc3f79
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3f7b vbe.c:542
    pop si                                    ; 5e                          ; 0xc3f7e
    pop cx                                    ; 59                          ; 0xc3f7f
    pop bx                                    ; 5b                          ; 0xc3f80
    pop bp                                    ; 5d                          ; 0xc3f81
    retn                                      ; c3                          ; 0xc3f82
  ; disGetNextSymbol 0xc3f83 LB 0x2ec -> off=0x0 cb=000000000000008f uValue=00000000000c3f83 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3f83 LB 0x8f
    push bp                                   ; 55                          ; 0xc3f83 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3f84
    push bx                                   ; 53                          ; 0xc3f86
    push cx                                   ; 51                          ; 0xc3f87
    push si                                   ; 56                          ; 0xc3f88
    push ax                                   ; 50                          ; 0xc3f89
    mov cx, ax                                ; 89 c1                       ; 0xc3f8a
    mov bx, dx                                ; 89 d3                       ; 0xc3f8c
    mov es, ax                                ; 8e c0                       ; 0xc3f8e vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3f90
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3f93
    inc bx                                    ; 43                          ; 0xc3f96 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3f97
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3f98 vbe.c:552
    jne short 03faeh                          ; 75 10                       ; 0xc3f9c
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3f9e vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fa1
    out DX, ax                                ; ef                          ; 0xc3fa4
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3fa5 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fa8
    out DX, ax                                ; ef                          ; 0xc3fab
    jmp short 0400ah                          ; eb 5c                       ; 0xc3fac vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3fae vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fb1
    out DX, ax                                ; ef                          ; 0xc3fb4
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fb5 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fb8 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fbb
    inc bx                                    ; 43                          ; 0xc3fbc vbe.c:558
    inc bx                                    ; 43                          ; 0xc3fbd
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3fbe
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fc1
    out DX, ax                                ; ef                          ; 0xc3fc4
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fc5 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fc8 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fcb
    inc bx                                    ; 43                          ; 0xc3fcc vbe.c:561
    inc bx                                    ; 43                          ; 0xc3fcd
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3fce
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fd1
    out DX, ax                                ; ef                          ; 0xc3fd4
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fd5 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fd8 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fdb
    inc bx                                    ; 43                          ; 0xc3fdc vbe.c:564
    inc bx                                    ; 43                          ; 0xc3fdd
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3fde
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fe1
    out DX, ax                                ; ef                          ; 0xc3fe4
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3fe5 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fe8
    out DX, ax                                ; ef                          ; 0xc3feb
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3fec vbe.c:568
    jmp short 03ff6h                          ; eb 05                       ; 0xc3fef
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3ff1
    jnbe short 0400ah                         ; 77 14                       ; 0xc3ff4
    mov ax, si                                ; 89 f0                       ; 0xc3ff6 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ff8
    out DX, ax                                ; ef                          ; 0xc3ffb
    mov es, cx                                ; 8e c1                       ; 0xc3ffc vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3ffe
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4001 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc4004
    inc bx                                    ; 43                          ; 0xc4005 vbe.c:571
    inc bx                                    ; 43                          ; 0xc4006
    inc si                                    ; 46                          ; 0xc4007 vbe.c:572
    jmp short 03ff1h                          ; eb e7                       ; 0xc4008
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc400a vbe.c:574
    pop si                                    ; 5e                          ; 0xc400d
    pop cx                                    ; 59                          ; 0xc400e
    pop bx                                    ; 5b                          ; 0xc400f
    pop bp                                    ; 5d                          ; 0xc4010
    retn                                      ; c3                          ; 0xc4011
  ; disGetNextSymbol 0xc4012 LB 0x25d -> off=0x0 cb=000000000000008c uValue=00000000000c4012 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc4012 LB 0x8c
    push bp                                   ; 55                          ; 0xc4012 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc4013
    push si                                   ; 56                          ; 0xc4015
    push di                                   ; 57                          ; 0xc4016
    push ax                                   ; 50                          ; 0xc4017
    mov si, ax                                ; 89 c6                       ; 0xc4018
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc401a
    mov ax, bx                                ; 89 d8                       ; 0xc401d
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc401f
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc4022 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc4025 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc4027
    je short 04071h                           ; 74 45                       ; 0xc402a
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc402c
    je short 04055h                           ; 74 24                       ; 0xc402f
    test ax, ax                               ; 85 c0                       ; 0xc4031
    jne short 0408dh                          ; 75 58                       ; 0xc4033
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4035 vbe.c:598
    call 02fe6h                               ; e8 ab ef                    ; 0xc4038
    mov cx, ax                                ; 89 c1                       ; 0xc403b
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc403d vbe.c:602
    je short 04048h                           ; 74 05                       ; 0xc4041
    call 03f30h                               ; e8 ea fe                    ; 0xc4043 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc4046
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc4048 vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc404b
    push SS                                   ; 16                          ; 0xc404e
    pop ES                                    ; 07                          ; 0xc404f
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4050
    jmp short 04090h                          ; eb 3b                       ; 0xc4053 vbe.c:605
    push SS                                   ; 16                          ; 0xc4055 vbe.c:607
    pop ES                                    ; 07                          ; 0xc4056
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4057
    mov dx, cx                                ; 89 ca                       ; 0xc405a vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc405c
    call 03021h                               ; e8 bf ef                    ; 0xc405f
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4062 vbe.c:612
    je short 04090h                           ; 74 28                       ; 0xc4066
    mov dx, ax                                ; 89 c2                       ; 0xc4068 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc406a
    call 03f38h                               ; e8 c9 fe                    ; 0xc406c
    jmp short 04090h                          ; eb 1f                       ; 0xc406f vbe.c:614
    push SS                                   ; 16                          ; 0xc4071 vbe.c:616
    pop ES                                    ; 07                          ; 0xc4072
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4073
    mov dx, cx                                ; 89 ca                       ; 0xc4076 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4078
    call 032f9h                               ; e8 7b f2                    ; 0xc407b
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc407e vbe.c:621
    je short 04090h                           ; 74 0c                       ; 0xc4082
    mov dx, ax                                ; 89 c2                       ; 0xc4084 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc4086
    call 03f83h                               ; e8 f8 fe                    ; 0xc4088
    jmp short 04090h                          ; eb 03                       ; 0xc408b vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc408d vbe.c:626
    push SS                                   ; 16                          ; 0xc4090 vbe.c:629
    pop ES                                    ; 07                          ; 0xc4091
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc4092
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4095 vbe.c:630
    pop di                                    ; 5f                          ; 0xc4098
    pop si                                    ; 5e                          ; 0xc4099
    pop bp                                    ; 5d                          ; 0xc409a
    retn 00002h                               ; c2 02 00                    ; 0xc409b
  ; disGetNextSymbol 0xc409e LB 0x1d1 -> off=0x0 cb=00000000000000df uValue=00000000000c409e 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc409e LB 0xdf
    push bp                                   ; 55                          ; 0xc409e vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc409f
    push si                                   ; 56                          ; 0xc40a1
    push di                                   ; 57                          ; 0xc40a2
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc40a3
    push ax                                   ; 50                          ; 0xc40a6
    mov di, dx                                ; 89 d7                       ; 0xc40a7
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc40a9
    mov si, cx                                ; 89 ce                       ; 0xc40ac
    call 03b42h                               ; e8 91 fa                    ; 0xc40ae vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc40b1 vbe.c:661
    jne short 040bah                          ; 75 05                       ; 0xc40b3
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc40b5
    jmp short 040beh                          ; eb 04                       ; 0xc40b8
    xor ah, ah                                ; 30 e4                       ; 0xc40ba
    mov bx, ax                                ; 89 c3                       ; 0xc40bc
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc40be
    call 03b7ah                               ; e8 b6 fa                    ; 0xc40c1 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc40c4
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc40c7 vbe.c:663
    push SS                                   ; 16                          ; 0xc40cc vbe.c:664
    pop ES                                    ; 07                          ; 0xc40cd
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc40ce
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc40d1
    mov cl, byte [es:di]                      ; 26 8a 0d                    ; 0xc40d4 vbe.c:665
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc40d7 vbe.c:669
    je short 040e8h                           ; 74 0c                       ; 0xc40da
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc40dc
    je short 0410eh                           ; 74 2d                       ; 0xc40df
    test cl, cl                               ; 84 c9                       ; 0xc40e1
    je short 04109h                           ; 74 24                       ; 0xc40e3
    jmp near 04166h                           ; e9 7e 00                    ; 0xc40e5
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc40e8 vbe.c:671
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc40eb
    jne short 040f4h                          ; 75 05                       ; 0xc40ed
    sal bx, 003h                              ; c1 e3 03                    ; 0xc40ef vbe.c:672
    jmp short 04109h                          ; eb 15                       ; 0xc40f2 vbe.c:673
    xor ah, ah                                ; 30 e4                       ; 0xc40f4 vbe.c:674
    cwd                                       ; 99                          ; 0xc40f6
    sal dx, 003h                              ; c1 e2 03                    ; 0xc40f7
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc40fa
    sar ax, 003h                              ; c1 f8 03                    ; 0xc40fc
    mov cx, ax                                ; 89 c1                       ; 0xc40ff
    mov ax, bx                                ; 89 d8                       ; 0xc4101
    xor dx, dx                                ; 31 d2                       ; 0xc4103
    div cx                                    ; f7 f1                       ; 0xc4105
    mov bx, ax                                ; 89 c3                       ; 0xc4107
    mov ax, bx                                ; 89 d8                       ; 0xc4109 vbe.c:677
    call 03b5bh                               ; e8 4d fa                    ; 0xc410b
    call 03b7ah                               ; e8 69 fa                    ; 0xc410e vbe.c:680
    mov cx, ax                                ; 89 c1                       ; 0xc4111
    push SS                                   ; 16                          ; 0xc4113 vbe.c:681
    pop ES                                    ; 07                          ; 0xc4114
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc4115
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4118
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc411b vbe.c:682
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc411e
    jne short 04129h                          ; 75 07                       ; 0xc4120
    mov bx, cx                                ; 89 cb                       ; 0xc4122 vbe.c:683
    shr bx, 003h                              ; c1 eb 03                    ; 0xc4124
    jmp short 0413ch                          ; eb 13                       ; 0xc4127 vbe.c:684
    xor ah, ah                                ; 30 e4                       ; 0xc4129 vbe.c:685
    cwd                                       ; 99                          ; 0xc412b
    sal dx, 003h                              ; c1 e2 03                    ; 0xc412c
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc412f
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4131
    mov bx, ax                                ; 89 c3                       ; 0xc4134
    mov ax, cx                                ; 89 c8                       ; 0xc4136
    mul bx                                    ; f7 e3                       ; 0xc4138
    mov bx, ax                                ; 89 c3                       ; 0xc413a
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc413c vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc413f
    push SS                                   ; 16                          ; 0xc4142 vbe.c:687
    pop ES                                    ; 07                          ; 0xc4143
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc4144
    call 03b93h                               ; e8 49 fa                    ; 0xc4147 vbe.c:688
    push SS                                   ; 16                          ; 0xc414a
    pop ES                                    ; 07                          ; 0xc414b
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc414c
    call 03b0ah                               ; e8 b8 f9                    ; 0xc414f vbe.c:689
    push SS                                   ; 16                          ; 0xc4152
    pop ES                                    ; 07                          ; 0xc4153
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc4154
    jbe short 0416bh                          ; 76 12                       ; 0xc4157
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4159 vbe.c:690
    call 03b5bh                               ; e8 fc f9                    ; 0xc415c
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc415f vbe.c:691
    jmp short 0416bh                          ; eb 05                       ; 0xc4164 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc4166 vbe.c:696
    push SS                                   ; 16                          ; 0xc416b vbe.c:699
    pop ES                                    ; 07                          ; 0xc416c
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc416d
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc4170
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4173
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4176 vbe.c:700
    pop di                                    ; 5f                          ; 0xc4179
    pop si                                    ; 5e                          ; 0xc417a
    pop bp                                    ; 5d                          ; 0xc417b
    retn                                      ; c3                          ; 0xc417c
  ; disGetNextSymbol 0xc417d LB 0xf2 -> off=0x0 cb=00000000000000f2 uValue=00000000000c417d 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc417d LB 0xf2
    push bp                                   ; 55                          ; 0xc417d vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc417e
    push si                                   ; 56                          ; 0xc4180
    push di                                   ; 57                          ; 0xc4181
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc4182
    mov di, ax                                ; 89 c7                       ; 0xc4185
    mov si, dx                                ; 89 d6                       ; 0xc4187
    mov dx, cx                                ; 89 ca                       ; 0xc4189
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc418b vbe.c:739
    push SS                                   ; 16                          ; 0xc4190 vbe.c:740
    pop ES                                    ; 07                          ; 0xc4191
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc4192
    test al, al                               ; 84 c0                       ; 0xc4195 vbe.c:741
    jne short 041bbh                          ; 75 22                       ; 0xc4197
    push SS                                   ; 16                          ; 0xc4199 vbe.c:743
    pop ES                                    ; 07                          ; 0xc419a
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc419b
    mov bx, dx                                ; 89 d3                       ; 0xc419e vbe.c:744
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc41a0
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc41a3 vbe.c:745
    shr ax, 008h                              ; c1 e8 08                    ; 0xc41a6
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc41a9
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc41ac
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc41af vbe.c:750
    je short 041c3h                           ; 74 10                       ; 0xc41b1
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc41b3
    je short 041c3h                           ; 74 0c                       ; 0xc41b5
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc41b7
    je short 041c3h                           ; 74 08                       ; 0xc41b9
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc41bb vbe.c:751
    jmp near 04260h                           ; e9 9d 00                    ; 0xc41c0 vbe.c:752
    push SS                                   ; 16                          ; 0xc41c3 vbe.c:756
    pop ES                                    ; 07                          ; 0xc41c4
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc41c5
    je short 041d1h                           ; 74 05                       ; 0xc41ca
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41cc
    jmp short 041d3h                          ; eb 02                       ; 0xc41cf
    xor ax, ax                                ; 31 c0                       ; 0xc41d1
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc41d3
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc41d6 vbe.c:759
    jnc short 041e1h                          ; 73 05                       ; 0xc41da
    mov cx, 00280h                            ; b9 80 02                    ; 0xc41dc vbe.c:760
    jmp short 041eah                          ; eb 09                       ; 0xc41df vbe.c:761
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc41e1
    jbe short 041eah                          ; 76 03                       ; 0xc41e5
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc41e7 vbe.c:762
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc41ea vbe.c:763
    jnc short 041f5h                          ; 73 05                       ; 0xc41ee
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc41f0 vbe.c:764
    jmp short 041feh                          ; eb 09                       ; 0xc41f3 vbe.c:765
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc41f5
    jbe short 041feh                          ; 76 03                       ; 0xc41f9
    mov bx, 00780h                            ; bb 80 07                    ; 0xc41fb vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc41fe vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4201
    call 03bach                               ; e8 a5 f9                    ; 0xc4204
    mov si, ax                                ; 89 c6                       ; 0xc4207
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc4209 vbe.c:775
    xor ah, ah                                ; 30 e4                       ; 0xc420c
    cwd                                       ; 99                          ; 0xc420e
    sal dx, 003h                              ; c1 e2 03                    ; 0xc420f
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4212
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4214
    mov dx, ax                                ; 89 c2                       ; 0xc4217
    mov ax, cx                                ; 89 c8                       ; 0xc4219
    mul dx                                    ; f7 e2                       ; 0xc421b
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc421d vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc4220
    mov dx, bx                                ; 89 da                       ; 0xc4222 vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc4224
    cmp dx, si                                ; 39 f2                       ; 0xc4226 vbe.c:780
    jnbe short 04230h                         ; 77 06                       ; 0xc4228
    jne short 04237h                          ; 75 0b                       ; 0xc422a
    test ax, ax                               ; 85 c0                       ; 0xc422c
    jbe short 04237h                          ; 76 07                       ; 0xc422e
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc4230 vbe.c:782
    jmp short 04260h                          ; eb 29                       ; 0xc4235 vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc4237 vbe.c:787
    call 005cdh                               ; e8 91 c3                    ; 0xc4239
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc423c vbe.c:788
    xor ah, ah                                ; 30 e4                       ; 0xc423f
    call 03b23h                               ; e8 df f8                    ; 0xc4241
    mov ax, cx                                ; 89 c8                       ; 0xc4244 vbe.c:789
    call 03acch                               ; e8 83 f8                    ; 0xc4246
    mov ax, bx                                ; 89 d8                       ; 0xc4249 vbe.c:790
    call 03aebh                               ; e8 9d f8                    ; 0xc424b
    xor ax, ax                                ; 31 c0                       ; 0xc424e vbe.c:791
    call 005f3h                               ; e8 a0 c3                    ; 0xc4250
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4253 vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc4256
    xor ah, ah                                ; 30 e4                       ; 0xc4258
    call 005cdh                               ; e8 70 c3                    ; 0xc425a
    call 006c2h                               ; e8 62 c4                    ; 0xc425d vbe.c:793
    push SS                                   ; 16                          ; 0xc4260 vbe.c:801
    pop ES                                    ; 07                          ; 0xc4261
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4262
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc4265
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4268 vbe.c:802
    pop di                                    ; 5f                          ; 0xc426b
    pop si                                    ; 5e                          ; 0xc426c
    pop bp                                    ; 5d                          ; 0xc426d
    retn                                      ; c3                          ; 0xc426e

  ; Padding 0x391 bytes at 0xc426f
  times 913 db 0

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
    db  061h, 042h, 069h, 06fh, 073h, 032h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h
    db  042h, 069h, 06fh, 073h, 032h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 088h
