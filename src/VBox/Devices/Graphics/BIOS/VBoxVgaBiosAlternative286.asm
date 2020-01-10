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





section VGAROM progbits vstart=0x0 align=1 ; size=0x8fd class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x8fd -> off=0x22 cb=000000000000054e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e3h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x54e
    pushfw                                    ; 9c                          ; 0xc0022 vgarom.asm:84
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0023 vgarom.asm:96
    jne short 0002eh                          ; 75 06                       ; 0xc0026 vgarom.asm:97
    call 0017ah                               ; e8 4f 01                    ; 0xc0028 vgarom.asm:98
    jmp near 000eah                           ; e9 bc 00                    ; 0xc002b vgarom.asm:99
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc002e vgarom.asm:101
    jne short 00039h                          ; 75 06                       ; 0xc0031 vgarom.asm:102
    call 0052fh                               ; e8 f9 04                    ; 0xc0033 vgarom.asm:103
    jmp near 000eah                           ; e9 b1 00                    ; 0xc0036 vgarom.asm:104
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc0039 vgarom.asm:106
    jne short 00044h                          ; 75 06                       ; 0xc003c vgarom.asm:107
    call 000ech                               ; e8 ab 00                    ; 0xc003e vgarom.asm:108
    jmp near 000eah                           ; e9 a6 00                    ; 0xc0041 vgarom.asm:109
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc0044 vgarom.asm:111
    jne short 0004fh                          ; 75 06                       ; 0xc0047 vgarom.asm:112
    call 00426h                               ; e8 da 03                    ; 0xc0049 vgarom.asm:113
    jmp near 000eah                           ; e9 9b 00                    ; 0xc004c vgarom.asm:114
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc004f vgarom.asm:116
    jne short 00092h                          ; 75 3e                       ; 0xc0052 vgarom.asm:117
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc0054 vgarom.asm:118
    jne short 0005fh                          ; 75 06                       ; 0xc0057 vgarom.asm:119
    call 00433h                               ; e8 d7 03                    ; 0xc0059 vgarom.asm:120
    jmp near 000eah                           ; e9 8b 00                    ; 0xc005c vgarom.asm:121
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc005f vgarom.asm:123
    jne short 0006ah                          ; 75 06                       ; 0xc0062 vgarom.asm:124
    call 00456h                               ; e8 ef 03                    ; 0xc0064 vgarom.asm:125
    jmp near 000eah                           ; e9 80 00                    ; 0xc0067 vgarom.asm:126
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc006a vgarom.asm:128
    jne short 00074h                          ; 75 05                       ; 0xc006d vgarom.asm:129
    call 004a9h                               ; e8 37 04                    ; 0xc006f vgarom.asm:130
    jmp short 000eah                          ; eb 76                       ; 0xc0072 vgarom.asm:131
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0074 vgarom.asm:133
    jne short 0007eh                          ; 75 05                       ; 0xc0077 vgarom.asm:134
    call 004cbh                               ; e8 4f 04                    ; 0xc0079 vgarom.asm:135
    jmp short 000eah                          ; eb 6c                       ; 0xc007c vgarom.asm:136
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc007e vgarom.asm:138
    jne short 00088h                          ; 75 05                       ; 0xc0081 vgarom.asm:139
    call 004e9h                               ; e8 63 04                    ; 0xc0083 vgarom.asm:140
    jmp short 000eah                          ; eb 62                       ; 0xc0086 vgarom.asm:141
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc0088 vgarom.asm:143
    jne short 000dch                          ; 75 4f                       ; 0xc008b vgarom.asm:144
    call 0050dh                               ; e8 7d 04                    ; 0xc008d vgarom.asm:145
    jmp short 000eah                          ; eb 58                       ; 0xc0090 vgarom.asm:146
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0092 vgarom.asm:148
    je short 000dch                           ; 74 45                       ; 0xc0095 vgarom.asm:149
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc0097 vgarom.asm:150
    jne short 000a1h                          ; 75 05                       ; 0xc009a vgarom.asm:154
    call 001a1h                               ; e8 02 01                    ; 0xc009c vgarom.asm:156
    jmp short 000eah                          ; eb 49                       ; 0xc009f vgarom.asm:157
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a1 vgarom.asm:160
    jne short 000dch                          ; 75 36                       ; 0xc00a4 vgarom.asm:161
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00a6 vgarom.asm:162
    jne short 000afh                          ; 75 05                       ; 0xc00a8 vgarom.asm:163
    call 007d2h                               ; e8 25 07                    ; 0xc00aa vgarom.asm:164
    jmp short 000eah                          ; eb 3b                       ; 0xc00ad vgarom.asm:165
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00af vgarom.asm:167
    jne short 000b8h                          ; 75 05                       ; 0xc00b1 vgarom.asm:168
    call 007f7h                               ; e8 41 07                    ; 0xc00b3 vgarom.asm:169
    jmp short 000eah                          ; eb 32                       ; 0xc00b6 vgarom.asm:170
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00b8 vgarom.asm:172
    jne short 000c1h                          ; 75 05                       ; 0xc00ba vgarom.asm:173
    call 00824h                               ; e8 65 07                    ; 0xc00bc vgarom.asm:174
    jmp short 000eah                          ; eb 29                       ; 0xc00bf vgarom.asm:175
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c1 vgarom.asm:177
    jne short 000cah                          ; 75 05                       ; 0xc00c3 vgarom.asm:178
    call 00858h                               ; e8 90 07                    ; 0xc00c5 vgarom.asm:179
    jmp short 000eah                          ; eb 20                       ; 0xc00c8 vgarom.asm:180
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00ca vgarom.asm:182
    jne short 000d3h                          ; 75 05                       ; 0xc00cc vgarom.asm:183
    call 0088fh                               ; e8 be 07                    ; 0xc00ce vgarom.asm:184
    jmp short 000eah                          ; eb 17                       ; 0xc00d1 vgarom.asm:185
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d3 vgarom.asm:187
    jne short 000dch                          ; 75 05                       ; 0xc00d5 vgarom.asm:188
    call 008e6h                               ; e8 0c 08                    ; 0xc00d7 vgarom.asm:189
    jmp short 000eah                          ; eb 0e                       ; 0xc00da vgarom.asm:190
    push ES                                   ; 06                          ; 0xc00dc vgarom.asm:194
    push DS                                   ; 1e                          ; 0xc00dd vgarom.asm:195
    pushaw                                    ; 60                          ; 0xc00de vgarom.asm:97
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc00df vgarom.asm:199
    mov ds, bx                                ; 8e db                       ; 0xc00e2 vgarom.asm:200
    call 0368eh                               ; e8 a7 35                    ; 0xc00e4 vgarom.asm:201
    popaw                                     ; 61                          ; 0xc00e7 vgarom.asm:114
    pop DS                                    ; 1f                          ; 0xc00e8 vgarom.asm:204
    pop ES                                    ; 07                          ; 0xc00e9 vgarom.asm:205
    popfw                                     ; 9d                          ; 0xc00ea vgarom.asm:207
    iret                                      ; cf                          ; 0xc00eb vgarom.asm:208
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00ec vgarom.asm:213
    je short 000f7h                           ; 74 06                       ; 0xc00ef vgarom.asm:214
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc00f1 vgarom.asm:215
    je short 00148h                           ; 74 52                       ; 0xc00f4 vgarom.asm:216
    retn                                      ; c3                          ; 0xc00f6 vgarom.asm:220
    push ax                                   ; 50                          ; 0xc00f7 vgarom.asm:222
    push bx                                   ; 53                          ; 0xc00f8 vgarom.asm:223
    push cx                                   ; 51                          ; 0xc00f9 vgarom.asm:224
    push dx                                   ; 52                          ; 0xc00fa vgarom.asm:225
    push DS                                   ; 1e                          ; 0xc00fb vgarom.asm:226
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc00fc vgarom.asm:227
    mov ds, dx                                ; 8e da                       ; 0xc00ff vgarom.asm:228
    mov dx, 003dah                            ; ba da 03                    ; 0xc0101 vgarom.asm:229
    in AL, DX                                 ; ec                          ; 0xc0104 vgarom.asm:230
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0105 vgarom.asm:231
    jbe short 0013bh                          ; 76 2f                       ; 0xc010a vgarom.asm:232
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc010c vgarom.asm:233
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc010f vgarom.asm:234
    out DX, AL                                ; ee                          ; 0xc0111 vgarom.asm:235
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0112 vgarom.asm:236
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0114 vgarom.asm:237
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0116 vgarom.asm:238
    je short 0011ch                           ; 74 02                       ; 0xc0118 vgarom.asm:239
    add AL, strict byte 008h                  ; 04 08                       ; 0xc011a vgarom.asm:240
    out DX, AL                                ; ee                          ; 0xc011c vgarom.asm:242
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc011d vgarom.asm:243
    and bl, 010h                              ; 80 e3 10                    ; 0xc011f vgarom.asm:244
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0122 vgarom.asm:246
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0125 vgarom.asm:247
    out DX, AL                                ; ee                          ; 0xc0127 vgarom.asm:248
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0128 vgarom.asm:249
    in AL, DX                                 ; ec                          ; 0xc012b vgarom.asm:250
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc012c vgarom.asm:251
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc012e vgarom.asm:252
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0130 vgarom.asm:253
    out DX, AL                                ; ee                          ; 0xc0133 vgarom.asm:254
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0134 vgarom.asm:255
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0136 vgarom.asm:256
    jne short 00122h                          ; 75 e7                       ; 0xc0139 vgarom.asm:257
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc013b vgarom.asm:259
    out DX, AL                                ; ee                          ; 0xc013d vgarom.asm:260
    mov dx, 003dah                            ; ba da 03                    ; 0xc013e vgarom.asm:262
    in AL, DX                                 ; ec                          ; 0xc0141 vgarom.asm:263
    pop DS                                    ; 1f                          ; 0xc0142 vgarom.asm:265
    pop dx                                    ; 5a                          ; 0xc0143 vgarom.asm:266
    pop cx                                    ; 59                          ; 0xc0144 vgarom.asm:267
    pop bx                                    ; 5b                          ; 0xc0145 vgarom.asm:268
    pop ax                                    ; 58                          ; 0xc0146 vgarom.asm:269
    retn                                      ; c3                          ; 0xc0147 vgarom.asm:270
    push ax                                   ; 50                          ; 0xc0148 vgarom.asm:272
    push bx                                   ; 53                          ; 0xc0149 vgarom.asm:273
    push cx                                   ; 51                          ; 0xc014a vgarom.asm:274
    push dx                                   ; 52                          ; 0xc014b vgarom.asm:275
    mov dx, 003dah                            ; ba da 03                    ; 0xc014c vgarom.asm:276
    in AL, DX                                 ; ec                          ; 0xc014f vgarom.asm:277
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0150 vgarom.asm:278
    and bl, 001h                              ; 80 e3 01                    ; 0xc0152 vgarom.asm:279
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0155 vgarom.asm:281
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0158 vgarom.asm:282
    out DX, AL                                ; ee                          ; 0xc015a vgarom.asm:283
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc015b vgarom.asm:284
    in AL, DX                                 ; ec                          ; 0xc015e vgarom.asm:285
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc015f vgarom.asm:286
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0161 vgarom.asm:287
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0163 vgarom.asm:288
    out DX, AL                                ; ee                          ; 0xc0166 vgarom.asm:289
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0167 vgarom.asm:290
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0169 vgarom.asm:291
    jne short 00155h                          ; 75 e7                       ; 0xc016c vgarom.asm:292
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc016e vgarom.asm:293
    out DX, AL                                ; ee                          ; 0xc0170 vgarom.asm:294
    mov dx, 003dah                            ; ba da 03                    ; 0xc0171 vgarom.asm:296
    in AL, DX                                 ; ec                          ; 0xc0174 vgarom.asm:297
    pop dx                                    ; 5a                          ; 0xc0175 vgarom.asm:299
    pop cx                                    ; 59                          ; 0xc0176 vgarom.asm:300
    pop bx                                    ; 5b                          ; 0xc0177 vgarom.asm:301
    pop ax                                    ; 58                          ; 0xc0178 vgarom.asm:302
    retn                                      ; c3                          ; 0xc0179 vgarom.asm:303
    push DS                                   ; 1e                          ; 0xc017a vgarom.asm:308
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc017b vgarom.asm:309
    mov ds, ax                                ; 8e d8                       ; 0xc017e vgarom.asm:310
    push bx                                   ; 53                          ; 0xc0180 vgarom.asm:311
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc0181 vgarom.asm:312
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0184 vgarom.asm:313
    pop bx                                    ; 5b                          ; 0xc0186 vgarom.asm:314
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0187 vgarom.asm:315
    push bx                                   ; 53                          ; 0xc0189 vgarom.asm:316
    mov bx, 00087h                            ; bb 87 00                    ; 0xc018a vgarom.asm:317
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc018d vgarom.asm:318
    and ah, 080h                              ; 80 e4 80                    ; 0xc018f vgarom.asm:319
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0192 vgarom.asm:320
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0195 vgarom.asm:321
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc0197 vgarom.asm:322
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0199 vgarom.asm:323
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc019c vgarom.asm:324
    pop bx                                    ; 5b                          ; 0xc019e vgarom.asm:325
    pop DS                                    ; 1f                          ; 0xc019f vgarom.asm:326
    retn                                      ; c3                          ; 0xc01a0 vgarom.asm:327
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01a1 vgarom.asm:332
    jne short 001a7h                          ; 75 02                       ; 0xc01a3 vgarom.asm:333
    jmp short 00208h                          ; eb 61                       ; 0xc01a5 vgarom.asm:334
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01a7 vgarom.asm:336
    jne short 001adh                          ; 75 02                       ; 0xc01a9 vgarom.asm:337
    jmp short 00226h                          ; eb 79                       ; 0xc01ab vgarom.asm:338
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01ad vgarom.asm:340
    jne short 001b3h                          ; 75 02                       ; 0xc01af vgarom.asm:341
    jmp short 0022eh                          ; eb 7b                       ; 0xc01b1 vgarom.asm:342
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01b3 vgarom.asm:344
    jne short 001bah                          ; 75 03                       ; 0xc01b5 vgarom.asm:345
    jmp near 0025fh                           ; e9 a5 00                    ; 0xc01b7 vgarom.asm:346
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01ba vgarom.asm:348
    jne short 001c1h                          ; 75 03                       ; 0xc01bc vgarom.asm:349
    jmp near 00289h                           ; e9 c8 00                    ; 0xc01be vgarom.asm:350
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01c1 vgarom.asm:352
    jne short 001c8h                          ; 75 03                       ; 0xc01c3 vgarom.asm:353
    jmp near 002b1h                           ; e9 e9 00                    ; 0xc01c5 vgarom.asm:354
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01c8 vgarom.asm:356
    jne short 001cfh                          ; 75 03                       ; 0xc01ca vgarom.asm:357
    jmp near 002bfh                           ; e9 f0 00                    ; 0xc01cc vgarom.asm:358
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01cf vgarom.asm:360
    jne short 001d6h                          ; 75 03                       ; 0xc01d1 vgarom.asm:361
    jmp near 00304h                           ; e9 2e 01                    ; 0xc01d3 vgarom.asm:362
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01d6 vgarom.asm:364
    jne short 001ddh                          ; 75 03                       ; 0xc01d8 vgarom.asm:365
    jmp near 0031dh                           ; e9 40 01                    ; 0xc01da vgarom.asm:366
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01dd vgarom.asm:368
    jne short 001e4h                          ; 75 03                       ; 0xc01df vgarom.asm:369
    jmp near 00345h                           ; e9 61 01                    ; 0xc01e1 vgarom.asm:370
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01e4 vgarom.asm:372
    jne short 001ebh                          ; 75 03                       ; 0xc01e6 vgarom.asm:373
    jmp near 0038ch                           ; e9 a1 01                    ; 0xc01e8 vgarom.asm:374
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01eb vgarom.asm:376
    jne short 001f2h                          ; 75 03                       ; 0xc01ed vgarom.asm:377
    jmp near 003a7h                           ; e9 b5 01                    ; 0xc01ef vgarom.asm:378
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc01f2 vgarom.asm:380
    jne short 001f9h                          ; 75 03                       ; 0xc01f4 vgarom.asm:381
    jmp near 003cfh                           ; e9 d6 01                    ; 0xc01f6 vgarom.asm:382
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc01f9 vgarom.asm:384
    jne short 00200h                          ; 75 03                       ; 0xc01fb vgarom.asm:385
    jmp near 003dah                           ; e9 da 01                    ; 0xc01fd vgarom.asm:386
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc0200 vgarom.asm:388
    jne short 00207h                          ; 75 03                       ; 0xc0202 vgarom.asm:389
    jmp near 003e5h                           ; e9 de 01                    ; 0xc0204 vgarom.asm:390
    retn                                      ; c3                          ; 0xc0207 vgarom.asm:395
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0208 vgarom.asm:398
    jnbe short 00225h                         ; 77 18                       ; 0xc020b vgarom.asm:399
    push ax                                   ; 50                          ; 0xc020d vgarom.asm:400
    push dx                                   ; 52                          ; 0xc020e vgarom.asm:401
    mov dx, 003dah                            ; ba da 03                    ; 0xc020f vgarom.asm:402
    in AL, DX                                 ; ec                          ; 0xc0212 vgarom.asm:403
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0213 vgarom.asm:404
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0216 vgarom.asm:405
    out DX, AL                                ; ee                          ; 0xc0218 vgarom.asm:406
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0219 vgarom.asm:407
    out DX, AL                                ; ee                          ; 0xc021b vgarom.asm:408
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc021c vgarom.asm:409
    out DX, AL                                ; ee                          ; 0xc021e vgarom.asm:410
    mov dx, 003dah                            ; ba da 03                    ; 0xc021f vgarom.asm:412
    in AL, DX                                 ; ec                          ; 0xc0222 vgarom.asm:413
    pop dx                                    ; 5a                          ; 0xc0223 vgarom.asm:415
    pop ax                                    ; 58                          ; 0xc0224 vgarom.asm:416
    retn                                      ; c3                          ; 0xc0225 vgarom.asm:418
    push bx                                   ; 53                          ; 0xc0226 vgarom.asm:423
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc0227 vgarom.asm:424
    call 00208h                               ; e8 dc ff                    ; 0xc0229 vgarom.asm:425
    pop bx                                    ; 5b                          ; 0xc022c vgarom.asm:426
    retn                                      ; c3                          ; 0xc022d vgarom.asm:427
    push ax                                   ; 50                          ; 0xc022e vgarom.asm:432
    push bx                                   ; 53                          ; 0xc022f vgarom.asm:433
    push cx                                   ; 51                          ; 0xc0230 vgarom.asm:434
    push dx                                   ; 52                          ; 0xc0231 vgarom.asm:435
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0232 vgarom.asm:436
    mov dx, 003dah                            ; ba da 03                    ; 0xc0234 vgarom.asm:437
    in AL, DX                                 ; ec                          ; 0xc0237 vgarom.asm:438
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc0238 vgarom.asm:439
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc023a vgarom.asm:440
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc023d vgarom.asm:442
    out DX, AL                                ; ee                          ; 0xc023f vgarom.asm:443
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0240 vgarom.asm:444
    out DX, AL                                ; ee                          ; 0xc0243 vgarom.asm:445
    inc bx                                    ; 43                          ; 0xc0244 vgarom.asm:446
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0245 vgarom.asm:447
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc0247 vgarom.asm:448
    jne short 0023dh                          ; 75 f1                       ; 0xc024a vgarom.asm:449
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc024c vgarom.asm:450
    out DX, AL                                ; ee                          ; 0xc024e vgarom.asm:451
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc024f vgarom.asm:452
    out DX, AL                                ; ee                          ; 0xc0252 vgarom.asm:453
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0253 vgarom.asm:454
    out DX, AL                                ; ee                          ; 0xc0255 vgarom.asm:455
    mov dx, 003dah                            ; ba da 03                    ; 0xc0256 vgarom.asm:457
    in AL, DX                                 ; ec                          ; 0xc0259 vgarom.asm:458
    pop dx                                    ; 5a                          ; 0xc025a vgarom.asm:460
    pop cx                                    ; 59                          ; 0xc025b vgarom.asm:461
    pop bx                                    ; 5b                          ; 0xc025c vgarom.asm:462
    pop ax                                    ; 58                          ; 0xc025d vgarom.asm:463
    retn                                      ; c3                          ; 0xc025e vgarom.asm:464
    push ax                                   ; 50                          ; 0xc025f vgarom.asm:469
    push bx                                   ; 53                          ; 0xc0260 vgarom.asm:470
    push dx                                   ; 52                          ; 0xc0261 vgarom.asm:471
    mov dx, 003dah                            ; ba da 03                    ; 0xc0262 vgarom.asm:472
    in AL, DX                                 ; ec                          ; 0xc0265 vgarom.asm:473
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0266 vgarom.asm:474
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0269 vgarom.asm:475
    out DX, AL                                ; ee                          ; 0xc026b vgarom.asm:476
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc026c vgarom.asm:477
    in AL, DX                                 ; ec                          ; 0xc026f vgarom.asm:478
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc0270 vgarom.asm:479
    and bl, 001h                              ; 80 e3 01                    ; 0xc0272 vgarom.asm:480
    sal bl, 003h                              ; c0 e3 03                    ; 0xc0275 vgarom.asm:482
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0278 vgarom.asm:488
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc027a vgarom.asm:489
    out DX, AL                                ; ee                          ; 0xc027d vgarom.asm:490
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc027e vgarom.asm:491
    out DX, AL                                ; ee                          ; 0xc0280 vgarom.asm:492
    mov dx, 003dah                            ; ba da 03                    ; 0xc0281 vgarom.asm:494
    in AL, DX                                 ; ec                          ; 0xc0284 vgarom.asm:495
    pop dx                                    ; 5a                          ; 0xc0285 vgarom.asm:497
    pop bx                                    ; 5b                          ; 0xc0286 vgarom.asm:498
    pop ax                                    ; 58                          ; 0xc0287 vgarom.asm:499
    retn                                      ; c3                          ; 0xc0288 vgarom.asm:500
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0289 vgarom.asm:505
    jnbe short 002b0h                         ; 77 22                       ; 0xc028c vgarom.asm:506
    push ax                                   ; 50                          ; 0xc028e vgarom.asm:507
    push dx                                   ; 52                          ; 0xc028f vgarom.asm:508
    mov dx, 003dah                            ; ba da 03                    ; 0xc0290 vgarom.asm:509
    in AL, DX                                 ; ec                          ; 0xc0293 vgarom.asm:510
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0294 vgarom.asm:511
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0297 vgarom.asm:512
    out DX, AL                                ; ee                          ; 0xc0299 vgarom.asm:513
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc029a vgarom.asm:514
    in AL, DX                                 ; ec                          ; 0xc029d vgarom.asm:515
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc029e vgarom.asm:516
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a0 vgarom.asm:517
    in AL, DX                                 ; ec                          ; 0xc02a3 vgarom.asm:518
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a4 vgarom.asm:519
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02a7 vgarom.asm:520
    out DX, AL                                ; ee                          ; 0xc02a9 vgarom.asm:521
    mov dx, 003dah                            ; ba da 03                    ; 0xc02aa vgarom.asm:523
    in AL, DX                                 ; ec                          ; 0xc02ad vgarom.asm:524
    pop dx                                    ; 5a                          ; 0xc02ae vgarom.asm:526
    pop ax                                    ; 58                          ; 0xc02af vgarom.asm:527
    retn                                      ; c3                          ; 0xc02b0 vgarom.asm:529
    push ax                                   ; 50                          ; 0xc02b1 vgarom.asm:534
    push bx                                   ; 53                          ; 0xc02b2 vgarom.asm:535
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02b3 vgarom.asm:536
    call 00289h                               ; e8 d1 ff                    ; 0xc02b5 vgarom.asm:537
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02b8 vgarom.asm:538
    pop bx                                    ; 5b                          ; 0xc02ba vgarom.asm:539
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02bb vgarom.asm:540
    pop ax                                    ; 58                          ; 0xc02bd vgarom.asm:541
    retn                                      ; c3                          ; 0xc02be vgarom.asm:542
    push ax                                   ; 50                          ; 0xc02bf vgarom.asm:547
    push bx                                   ; 53                          ; 0xc02c0 vgarom.asm:548
    push cx                                   ; 51                          ; 0xc02c1 vgarom.asm:549
    push dx                                   ; 52                          ; 0xc02c2 vgarom.asm:550
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02c3 vgarom.asm:551
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02c5 vgarom.asm:552
    mov dx, 003dah                            ; ba da 03                    ; 0xc02c7 vgarom.asm:554
    in AL, DX                                 ; ec                          ; 0xc02ca vgarom.asm:555
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02cb vgarom.asm:556
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02ce vgarom.asm:557
    out DX, AL                                ; ee                          ; 0xc02d0 vgarom.asm:558
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02d1 vgarom.asm:559
    in AL, DX                                 ; ec                          ; 0xc02d4 vgarom.asm:560
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02d5 vgarom.asm:561
    inc bx                                    ; 43                          ; 0xc02d8 vgarom.asm:562
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02d9 vgarom.asm:563
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02db vgarom.asm:564
    jne short 002c7h                          ; 75 e7                       ; 0xc02de vgarom.asm:565
    mov dx, 003dah                            ; ba da 03                    ; 0xc02e0 vgarom.asm:566
    in AL, DX                                 ; ec                          ; 0xc02e3 vgarom.asm:567
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02e4 vgarom.asm:568
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02e7 vgarom.asm:569
    out DX, AL                                ; ee                          ; 0xc02e9 vgarom.asm:570
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02ea vgarom.asm:571
    in AL, DX                                 ; ec                          ; 0xc02ed vgarom.asm:572
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02ee vgarom.asm:573
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f1 vgarom.asm:574
    in AL, DX                                 ; ec                          ; 0xc02f4 vgarom.asm:575
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f5 vgarom.asm:576
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02f8 vgarom.asm:577
    out DX, AL                                ; ee                          ; 0xc02fa vgarom.asm:578
    mov dx, 003dah                            ; ba da 03                    ; 0xc02fb vgarom.asm:580
    in AL, DX                                 ; ec                          ; 0xc02fe vgarom.asm:581
    pop dx                                    ; 5a                          ; 0xc02ff vgarom.asm:583
    pop cx                                    ; 59                          ; 0xc0300 vgarom.asm:584
    pop bx                                    ; 5b                          ; 0xc0301 vgarom.asm:585
    pop ax                                    ; 58                          ; 0xc0302 vgarom.asm:586
    retn                                      ; c3                          ; 0xc0303 vgarom.asm:587
    push ax                                   ; 50                          ; 0xc0304 vgarom.asm:592
    push dx                                   ; 52                          ; 0xc0305 vgarom.asm:593
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0306 vgarom.asm:594
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0309 vgarom.asm:595
    out DX, AL                                ; ee                          ; 0xc030b vgarom.asm:596
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc030c vgarom.asm:597
    pop ax                                    ; 58                          ; 0xc030f vgarom.asm:598
    push ax                                   ; 50                          ; 0xc0310 vgarom.asm:599
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0311 vgarom.asm:600
    out DX, AL                                ; ee                          ; 0xc0313 vgarom.asm:601
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0314 vgarom.asm:602
    out DX, AL                                ; ee                          ; 0xc0316 vgarom.asm:603
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0317 vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc0319 vgarom.asm:605
    pop dx                                    ; 5a                          ; 0xc031a vgarom.asm:606
    pop ax                                    ; 58                          ; 0xc031b vgarom.asm:607
    retn                                      ; c3                          ; 0xc031c vgarom.asm:608
    push ax                                   ; 50                          ; 0xc031d vgarom.asm:613
    push bx                                   ; 53                          ; 0xc031e vgarom.asm:614
    push cx                                   ; 51                          ; 0xc031f vgarom.asm:615
    push dx                                   ; 52                          ; 0xc0320 vgarom.asm:616
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0321 vgarom.asm:617
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0324 vgarom.asm:618
    out DX, AL                                ; ee                          ; 0xc0326 vgarom.asm:619
    pop dx                                    ; 5a                          ; 0xc0327 vgarom.asm:620
    push dx                                   ; 52                          ; 0xc0328 vgarom.asm:621
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0329 vgarom.asm:622
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc032b vgarom.asm:623
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc032e vgarom.asm:625
    out DX, AL                                ; ee                          ; 0xc0331 vgarom.asm:626
    inc bx                                    ; 43                          ; 0xc0332 vgarom.asm:627
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0333 vgarom.asm:628
    out DX, AL                                ; ee                          ; 0xc0336 vgarom.asm:629
    inc bx                                    ; 43                          ; 0xc0337 vgarom.asm:630
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0338 vgarom.asm:631
    out DX, AL                                ; ee                          ; 0xc033b vgarom.asm:632
    inc bx                                    ; 43                          ; 0xc033c vgarom.asm:633
    dec cx                                    ; 49                          ; 0xc033d vgarom.asm:634
    jne short 0032eh                          ; 75 ee                       ; 0xc033e vgarom.asm:635
    pop dx                                    ; 5a                          ; 0xc0340 vgarom.asm:636
    pop cx                                    ; 59                          ; 0xc0341 vgarom.asm:637
    pop bx                                    ; 5b                          ; 0xc0342 vgarom.asm:638
    pop ax                                    ; 58                          ; 0xc0343 vgarom.asm:639
    retn                                      ; c3                          ; 0xc0344 vgarom.asm:640
    push ax                                   ; 50                          ; 0xc0345 vgarom.asm:645
    push bx                                   ; 53                          ; 0xc0346 vgarom.asm:646
    push dx                                   ; 52                          ; 0xc0347 vgarom.asm:647
    mov dx, 003dah                            ; ba da 03                    ; 0xc0348 vgarom.asm:648
    in AL, DX                                 ; ec                          ; 0xc034b vgarom.asm:649
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc034c vgarom.asm:650
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc034f vgarom.asm:651
    out DX, AL                                ; ee                          ; 0xc0351 vgarom.asm:652
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0352 vgarom.asm:653
    in AL, DX                                 ; ec                          ; 0xc0355 vgarom.asm:654
    and bl, 001h                              ; 80 e3 01                    ; 0xc0356 vgarom.asm:655
    jne short 00368h                          ; 75 0d                       ; 0xc0359 vgarom.asm:656
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc035b vgarom.asm:657
    sal bh, 007h                              ; c0 e7 07                    ; 0xc035d vgarom.asm:659
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc0360 vgarom.asm:669
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0362 vgarom.asm:670
    out DX, AL                                ; ee                          ; 0xc0365 vgarom.asm:671
    jmp short 00381h                          ; eb 19                       ; 0xc0366 vgarom.asm:672
    push ax                                   ; 50                          ; 0xc0368 vgarom.asm:674
    mov dx, 003dah                            ; ba da 03                    ; 0xc0369 vgarom.asm:675
    in AL, DX                                 ; ec                          ; 0xc036c vgarom.asm:676
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc036d vgarom.asm:677
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0370 vgarom.asm:678
    out DX, AL                                ; ee                          ; 0xc0372 vgarom.asm:679
    pop ax                                    ; 58                          ; 0xc0373 vgarom.asm:680
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0374 vgarom.asm:681
    jne short 0037bh                          ; 75 03                       ; 0xc0376 vgarom.asm:682
    sal bh, 002h                              ; c0 e7 02                    ; 0xc0378 vgarom.asm:684
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc037b vgarom.asm:690
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc037e vgarom.asm:691
    out DX, AL                                ; ee                          ; 0xc0380 vgarom.asm:692
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0381 vgarom.asm:694
    out DX, AL                                ; ee                          ; 0xc0383 vgarom.asm:695
    mov dx, 003dah                            ; ba da 03                    ; 0xc0384 vgarom.asm:697
    in AL, DX                                 ; ec                          ; 0xc0387 vgarom.asm:698
    pop dx                                    ; 5a                          ; 0xc0388 vgarom.asm:700
    pop bx                                    ; 5b                          ; 0xc0389 vgarom.asm:701
    pop ax                                    ; 58                          ; 0xc038a vgarom.asm:702
    retn                                      ; c3                          ; 0xc038b vgarom.asm:703
    push ax                                   ; 50                          ; 0xc038c vgarom.asm:708
    push dx                                   ; 52                          ; 0xc038d vgarom.asm:709
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc038e vgarom.asm:710
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0391 vgarom.asm:711
    out DX, AL                                ; ee                          ; 0xc0393 vgarom.asm:712
    pop ax                                    ; 58                          ; 0xc0394 vgarom.asm:713
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0395 vgarom.asm:714
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0397 vgarom.asm:715
    in AL, DX                                 ; ec                          ; 0xc039a vgarom.asm:716
    xchg al, ah                               ; 86 e0                       ; 0xc039b vgarom.asm:717
    push ax                                   ; 50                          ; 0xc039d vgarom.asm:718
    in AL, DX                                 ; ec                          ; 0xc039e vgarom.asm:719
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc039f vgarom.asm:720
    in AL, DX                                 ; ec                          ; 0xc03a1 vgarom.asm:721
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03a2 vgarom.asm:722
    pop dx                                    ; 5a                          ; 0xc03a4 vgarom.asm:723
    pop ax                                    ; 58                          ; 0xc03a5 vgarom.asm:724
    retn                                      ; c3                          ; 0xc03a6 vgarom.asm:725
    push ax                                   ; 50                          ; 0xc03a7 vgarom.asm:730
    push bx                                   ; 53                          ; 0xc03a8 vgarom.asm:731
    push cx                                   ; 51                          ; 0xc03a9 vgarom.asm:732
    push dx                                   ; 52                          ; 0xc03aa vgarom.asm:733
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03ab vgarom.asm:734
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03ae vgarom.asm:735
    out DX, AL                                ; ee                          ; 0xc03b0 vgarom.asm:736
    pop dx                                    ; 5a                          ; 0xc03b1 vgarom.asm:737
    push dx                                   ; 52                          ; 0xc03b2 vgarom.asm:738
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03b3 vgarom.asm:739
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b5 vgarom.asm:740
    in AL, DX                                 ; ec                          ; 0xc03b8 vgarom.asm:742
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03b9 vgarom.asm:743
    inc bx                                    ; 43                          ; 0xc03bc vgarom.asm:744
    in AL, DX                                 ; ec                          ; 0xc03bd vgarom.asm:745
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03be vgarom.asm:746
    inc bx                                    ; 43                          ; 0xc03c1 vgarom.asm:747
    in AL, DX                                 ; ec                          ; 0xc03c2 vgarom.asm:748
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03c3 vgarom.asm:749
    inc bx                                    ; 43                          ; 0xc03c6 vgarom.asm:750
    dec cx                                    ; 49                          ; 0xc03c7 vgarom.asm:751
    jne short 003b8h                          ; 75 ee                       ; 0xc03c8 vgarom.asm:752
    pop dx                                    ; 5a                          ; 0xc03ca vgarom.asm:753
    pop cx                                    ; 59                          ; 0xc03cb vgarom.asm:754
    pop bx                                    ; 5b                          ; 0xc03cc vgarom.asm:755
    pop ax                                    ; 58                          ; 0xc03cd vgarom.asm:756
    retn                                      ; c3                          ; 0xc03ce vgarom.asm:757
    push ax                                   ; 50                          ; 0xc03cf vgarom.asm:762
    push dx                                   ; 52                          ; 0xc03d0 vgarom.asm:763
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03d1 vgarom.asm:764
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03d4 vgarom.asm:765
    out DX, AL                                ; ee                          ; 0xc03d6 vgarom.asm:766
    pop dx                                    ; 5a                          ; 0xc03d7 vgarom.asm:767
    pop ax                                    ; 58                          ; 0xc03d8 vgarom.asm:768
    retn                                      ; c3                          ; 0xc03d9 vgarom.asm:769
    push ax                                   ; 50                          ; 0xc03da vgarom.asm:774
    push dx                                   ; 52                          ; 0xc03db vgarom.asm:775
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03dc vgarom.asm:776
    in AL, DX                                 ; ec                          ; 0xc03df vgarom.asm:777
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03e0 vgarom.asm:778
    pop dx                                    ; 5a                          ; 0xc03e2 vgarom.asm:779
    pop ax                                    ; 58                          ; 0xc03e3 vgarom.asm:780
    retn                                      ; c3                          ; 0xc03e4 vgarom.asm:781
    push ax                                   ; 50                          ; 0xc03e5 vgarom.asm:786
    push dx                                   ; 52                          ; 0xc03e6 vgarom.asm:787
    mov dx, 003dah                            ; ba da 03                    ; 0xc03e7 vgarom.asm:788
    in AL, DX                                 ; ec                          ; 0xc03ea vgarom.asm:789
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03eb vgarom.asm:790
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc03ee vgarom.asm:791
    out DX, AL                                ; ee                          ; 0xc03f0 vgarom.asm:792
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc03f1 vgarom.asm:793
    in AL, DX                                 ; ec                          ; 0xc03f4 vgarom.asm:794
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03f5 vgarom.asm:795
    shr bl, 007h                              ; c0 eb 07                    ; 0xc03f7 vgarom.asm:797
    mov dx, 003dah                            ; ba da 03                    ; 0xc03fa vgarom.asm:807
    in AL, DX                                 ; ec                          ; 0xc03fd vgarom.asm:808
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03fe vgarom.asm:809
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0401 vgarom.asm:810
    out DX, AL                                ; ee                          ; 0xc0403 vgarom.asm:811
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0404 vgarom.asm:812
    in AL, DX                                 ; ec                          ; 0xc0407 vgarom.asm:813
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0408 vgarom.asm:814
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc040a vgarom.asm:815
    test bl, 001h                             ; f6 c3 01                    ; 0xc040d vgarom.asm:816
    jne short 00415h                          ; 75 03                       ; 0xc0410 vgarom.asm:817
    shr bh, 002h                              ; c0 ef 02                    ; 0xc0412 vgarom.asm:819
    mov dx, 003dah                            ; ba da 03                    ; 0xc0415 vgarom.asm:825
    in AL, DX                                 ; ec                          ; 0xc0418 vgarom.asm:826
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0419 vgarom.asm:827
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc041c vgarom.asm:828
    out DX, AL                                ; ee                          ; 0xc041e vgarom.asm:829
    mov dx, 003dah                            ; ba da 03                    ; 0xc041f vgarom.asm:831
    in AL, DX                                 ; ec                          ; 0xc0422 vgarom.asm:832
    pop dx                                    ; 5a                          ; 0xc0423 vgarom.asm:834
    pop ax                                    ; 58                          ; 0xc0424 vgarom.asm:835
    retn                                      ; c3                          ; 0xc0425 vgarom.asm:836
    push ax                                   ; 50                          ; 0xc0426 vgarom.asm:841
    push dx                                   ; 52                          ; 0xc0427 vgarom.asm:842
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0428 vgarom.asm:843
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc042b vgarom.asm:844
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc042d vgarom.asm:845
    out DX, ax                                ; ef                          ; 0xc042f vgarom.asm:846
    pop dx                                    ; 5a                          ; 0xc0430 vgarom.asm:847
    pop ax                                    ; 58                          ; 0xc0431 vgarom.asm:848
    retn                                      ; c3                          ; 0xc0432 vgarom.asm:849
    push DS                                   ; 1e                          ; 0xc0433 vgarom.asm:854
    push ax                                   ; 50                          ; 0xc0434 vgarom.asm:855
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0435 vgarom.asm:856
    mov ds, ax                                ; 8e d8                       ; 0xc0438 vgarom.asm:857
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc043a vgarom.asm:858
    mov bx, 00088h                            ; bb 88 00                    ; 0xc043c vgarom.asm:859
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc043f vgarom.asm:860
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc0441 vgarom.asm:861
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc0444 vgarom.asm:862
    mov ax, word [bx]                         ; 8b 07                       ; 0xc0447 vgarom.asm:863
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0449 vgarom.asm:864
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc044c vgarom.asm:865
    jne short 00453h                          ; 75 02                       ; 0xc044f vgarom.asm:866
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc0451 vgarom.asm:867
    pop ax                                    ; 58                          ; 0xc0453 vgarom.asm:869
    pop DS                                    ; 1f                          ; 0xc0454 vgarom.asm:870
    retn                                      ; c3                          ; 0xc0455 vgarom.asm:871
    push DS                                   ; 1e                          ; 0xc0456 vgarom.asm:879
    push bx                                   ; 53                          ; 0xc0457 vgarom.asm:880
    push dx                                   ; 52                          ; 0xc0458 vgarom.asm:881
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0459 vgarom.asm:882
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc045b vgarom.asm:883
    mov ds, ax                                ; 8e d8                       ; 0xc045e vgarom.asm:884
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0460 vgarom.asm:885
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0463 vgarom.asm:886
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0465 vgarom.asm:887
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0468 vgarom.asm:888
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc046a vgarom.asm:889
    je short 00484h                           ; 74 15                       ; 0xc046d vgarom.asm:890
    jc short 0048eh                           ; 72 1d                       ; 0xc046f vgarom.asm:891
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc0471 vgarom.asm:892
    je short 00478h                           ; 74 02                       ; 0xc0474 vgarom.asm:893
    jmp short 004a2h                          ; eb 2a                       ; 0xc0476 vgarom.asm:903
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc0478 vgarom.asm:909
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc047a vgarom.asm:910
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc047c vgarom.asm:911
    or ah, 009h                               ; 80 cc 09                    ; 0xc047f vgarom.asm:912
    jne short 00498h                          ; 75 14                       ; 0xc0482 vgarom.asm:913
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc0484 vgarom.asm:919
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0486 vgarom.asm:920
    or ah, 009h                               ; 80 cc 09                    ; 0xc0489 vgarom.asm:921
    jne short 00498h                          ; 75 0a                       ; 0xc048c vgarom.asm:922
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc048e vgarom.asm:928
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc0490 vgarom.asm:929
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0492 vgarom.asm:930
    or ah, 008h                               ; 80 cc 08                    ; 0xc0495 vgarom.asm:931
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0498 vgarom.asm:933
    mov byte [bx], al                         ; 88 07                       ; 0xc049b vgarom.asm:934
    mov bx, 00088h                            ; bb 88 00                    ; 0xc049d vgarom.asm:935
    mov byte [bx], ah                         ; 88 27                       ; 0xc04a0 vgarom.asm:936
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04a2 vgarom.asm:938
    pop dx                                    ; 5a                          ; 0xc04a5 vgarom.asm:939
    pop bx                                    ; 5b                          ; 0xc04a6 vgarom.asm:940
    pop DS                                    ; 1f                          ; 0xc04a7 vgarom.asm:941
    retn                                      ; c3                          ; 0xc04a8 vgarom.asm:942
    push DS                                   ; 1e                          ; 0xc04a9 vgarom.asm:951
    push bx                                   ; 53                          ; 0xc04aa vgarom.asm:952
    push dx                                   ; 52                          ; 0xc04ab vgarom.asm:953
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04ac vgarom.asm:954
    and dl, 001h                              ; 80 e2 01                    ; 0xc04ae vgarom.asm:955
    sal dl, 003h                              ; c0 e2 03                    ; 0xc04b1 vgarom.asm:957
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04b4 vgarom.asm:963
    mov ds, ax                                ; 8e d8                       ; 0xc04b7 vgarom.asm:964
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04b9 vgarom.asm:965
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04bc vgarom.asm:966
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04be vgarom.asm:967
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04c0 vgarom.asm:968
    mov byte [bx], al                         ; 88 07                       ; 0xc04c2 vgarom.asm:969
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04c4 vgarom.asm:970
    pop dx                                    ; 5a                          ; 0xc04c7 vgarom.asm:971
    pop bx                                    ; 5b                          ; 0xc04c8 vgarom.asm:972
    pop DS                                    ; 1f                          ; 0xc04c9 vgarom.asm:973
    retn                                      ; c3                          ; 0xc04ca vgarom.asm:974
    push bx                                   ; 53                          ; 0xc04cb vgarom.asm:978
    push dx                                   ; 52                          ; 0xc04cc vgarom.asm:979
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04cd vgarom.asm:980
    and bl, 001h                              ; 80 e3 01                    ; 0xc04cf vgarom.asm:981
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04d2 vgarom.asm:982
    sal bl, 1                                 ; d0 e3                       ; 0xc04d5 vgarom.asm:983
    mov dx, 003cch                            ; ba cc 03                    ; 0xc04d7 vgarom.asm:984
    in AL, DX                                 ; ec                          ; 0xc04da vgarom.asm:985
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc04db vgarom.asm:986
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc04dd vgarom.asm:987
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc04df vgarom.asm:988
    out DX, AL                                ; ee                          ; 0xc04e2 vgarom.asm:989
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04e3 vgarom.asm:990
    pop dx                                    ; 5a                          ; 0xc04e6 vgarom.asm:991
    pop bx                                    ; 5b                          ; 0xc04e7 vgarom.asm:992
    retn                                      ; c3                          ; 0xc04e8 vgarom.asm:993
    push DS                                   ; 1e                          ; 0xc04e9 vgarom.asm:997
    push bx                                   ; 53                          ; 0xc04ea vgarom.asm:998
    push dx                                   ; 52                          ; 0xc04eb vgarom.asm:999
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04ec vgarom.asm:1000
    and dl, 001h                              ; 80 e2 01                    ; 0xc04ee vgarom.asm:1001
    xor dl, 001h                              ; 80 f2 01                    ; 0xc04f1 vgarom.asm:1002
    sal dl, 1                                 ; d0 e2                       ; 0xc04f4 vgarom.asm:1003
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04f6 vgarom.asm:1004
    mov ds, ax                                ; 8e d8                       ; 0xc04f9 vgarom.asm:1005
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04fb vgarom.asm:1006
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04fe vgarom.asm:1007
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0500 vgarom.asm:1008
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0502 vgarom.asm:1009
    mov byte [bx], al                         ; 88 07                       ; 0xc0504 vgarom.asm:1010
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0506 vgarom.asm:1011
    pop dx                                    ; 5a                          ; 0xc0509 vgarom.asm:1012
    pop bx                                    ; 5b                          ; 0xc050a vgarom.asm:1013
    pop DS                                    ; 1f                          ; 0xc050b vgarom.asm:1014
    retn                                      ; c3                          ; 0xc050c vgarom.asm:1015
    push DS                                   ; 1e                          ; 0xc050d vgarom.asm:1019
    push bx                                   ; 53                          ; 0xc050e vgarom.asm:1020
    push dx                                   ; 52                          ; 0xc050f vgarom.asm:1021
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0510 vgarom.asm:1022
    and dl, 001h                              ; 80 e2 01                    ; 0xc0512 vgarom.asm:1023
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0515 vgarom.asm:1024
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0518 vgarom.asm:1025
    mov ds, ax                                ; 8e d8                       ; 0xc051b vgarom.asm:1026
    mov bx, 00089h                            ; bb 89 00                    ; 0xc051d vgarom.asm:1027
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0520 vgarom.asm:1028
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0522 vgarom.asm:1029
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0524 vgarom.asm:1030
    mov byte [bx], al                         ; 88 07                       ; 0xc0526 vgarom.asm:1031
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0528 vgarom.asm:1032
    pop dx                                    ; 5a                          ; 0xc052b vgarom.asm:1033
    pop bx                                    ; 5b                          ; 0xc052c vgarom.asm:1034
    pop DS                                    ; 1f                          ; 0xc052d vgarom.asm:1035
    retn                                      ; c3                          ; 0xc052e vgarom.asm:1036
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc052f vgarom.asm:1041
    je short 00538h                           ; 74 05                       ; 0xc0531 vgarom.asm:1042
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0533 vgarom.asm:1043
    je short 0054dh                           ; 74 16                       ; 0xc0535 vgarom.asm:1044
    retn                                      ; c3                          ; 0xc0537 vgarom.asm:1048
    push DS                                   ; 1e                          ; 0xc0538 vgarom.asm:1050
    push ax                                   ; 50                          ; 0xc0539 vgarom.asm:1051
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc053a vgarom.asm:1052
    mov ds, ax                                ; 8e d8                       ; 0xc053d vgarom.asm:1053
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc053f vgarom.asm:1054
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0542 vgarom.asm:1055
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0544 vgarom.asm:1056
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0546 vgarom.asm:1057
    pop ax                                    ; 58                          ; 0xc0548 vgarom.asm:1058
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0549 vgarom.asm:1059
    pop DS                                    ; 1f                          ; 0xc054b vgarom.asm:1060
    retn                                      ; c3                          ; 0xc054c vgarom.asm:1061
    push DS                                   ; 1e                          ; 0xc054d vgarom.asm:1063
    push ax                                   ; 50                          ; 0xc054e vgarom.asm:1064
    push bx                                   ; 53                          ; 0xc054f vgarom.asm:1065
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0550 vgarom.asm:1066
    mov ds, ax                                ; 8e d8                       ; 0xc0553 vgarom.asm:1067
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0555 vgarom.asm:1068
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0557 vgarom.asm:1069
    mov byte [bx], al                         ; 88 07                       ; 0xc055a vgarom.asm:1070
    pop bx                                    ; 5b                          ; 0xc055c vgarom.asm:1080
    pop ax                                    ; 58                          ; 0xc055d vgarom.asm:1081
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc055e vgarom.asm:1082
    pop DS                                    ; 1f                          ; 0xc0560 vgarom.asm:1083
    retn                                      ; c3                          ; 0xc0561 vgarom.asm:1084
    times 0xe db 0
  ; disGetNextSymbol 0xc0570 LB 0x38d -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x386 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05b7 LB 0x346 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc05dd LB 0x320 -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc0603 LB 0x2fa -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc0629 LB 0x2d4 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06d2 LB 0x22b -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07bf LB 0x13e -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07d2 LB 0x12b -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc07f7 LB 0x106 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc0824 LB 0xd9 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0858 LB 0xa5 -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc088f LB 0x6e -> off=0x0 cb=0000000000000057 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x57
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008c2h                           ; 74 2a                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008e2h                          ; 76 45                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008deh                          ; 75 3c                       ; 0xc08a0 vberom.asm:690
    pushaw                                    ; 60                          ; 0xc08a2 vberom.asm:133
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
    popaw                                     ; 61                          ; 0xc08bd vberom.asm:152
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08be vberom.asm:727
    retn                                      ; c3                          ; 0xc08c1 vberom.asm:728
    pushaw                                    ; 60                          ; 0xc08c2 vberom.asm:133
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
    popaw                                     ; 61                          ; 0xc08db vberom.asm:152
    jmp short 008beh                          ; eb e0                       ; 0xc08dc vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08de vberom.asm:762
    retn                                      ; c3                          ; 0xc08e1 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08e2 vberom.asm:765
    retn                                      ; c3                          ; 0xc08e5 vberom.asm:766
  ; disGetNextSymbol 0xc08e6 LB 0x17 -> off=0x0 cb=0000000000000017 uValue=00000000000c08e6 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08e6 LB 0x17
    test bl, bl                               ; 84 db                       ; 0xc08e6 vberom.asm:780
    jne short 008f9h                          ; 75 0f                       ; 0xc08e8 vberom.asm:781
    mov di, 0c000h                            ; bf 00 c0                    ; 0xc08ea vberom.asm:782
    mov es, di                                ; 8e c7                       ; 0xc08ed vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc08ef vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08f2 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08f5 vberom.asm:786
    retn                                      ; c3                          ; 0xc08f8 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08f9 vberom.asm:789
    retn                                      ; c3                          ; 0xc08fc vberom.asm:790

  ; Padding 0x83 bytes at 0xc08fd
  times 131 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x3903 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x3903 -> off=0x0 cb=000000000000001b uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1b
    push bx                                   ; 53                          ; 0xc0980 vgabios.c:87
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    mov bl, al                                ; 88 c3                       ; 0xc0984
    xor bh, bh                                ; 30 ff                       ; 0xc0986 vgabios.c:91
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0988
    xor ax, ax                                ; 31 c0                       ; 0xc098b
    mov es, ax                                ; 8e c0                       ; 0xc098d
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc098f
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0992
    pop bp                                    ; 5d                          ; 0xc0998 vgabios.c:92
    pop bx                                    ; 5b                          ; 0xc0999
    retn                                      ; c3                          ; 0xc099a
  ; disGetNextSymbol 0xc099b LB 0x38e8 -> off=0x0 cb=000000000000001c uValue=00000000000c099b 'init_vga_card'
init_vga_card:                               ; 0xc099b LB 0x1c
    push bp                                   ; 55                          ; 0xc099b vgabios.c:143
    mov bp, sp                                ; 89 e5                       ; 0xc099c
    push dx                                   ; 52                          ; 0xc099e
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc099f vgabios.c:146
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a1
    out DX, AL                                ; ee                          ; 0xc09a4
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a5 vgabios.c:149
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a7
    out DX, AL                                ; ee                          ; 0xc09aa
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09ab vgabios.c:150
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ad
    out DX, AL                                ; ee                          ; 0xc09b0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b1 vgabios.c:155
    pop dx                                    ; 5a                          ; 0xc09b4
    pop bp                                    ; 5d                          ; 0xc09b5
    retn                                      ; c3                          ; 0xc09b6
  ; disGetNextSymbol 0xc09b7 LB 0x38cc -> off=0x0 cb=0000000000000032 uValue=00000000000c09b7 'init_bios_area'
init_bios_area:                              ; 0xc09b7 LB 0x32
    push bx                                   ; 53                          ; 0xc09b7 vgabios.c:164
    push bp                                   ; 55                          ; 0xc09b8
    mov bp, sp                                ; 89 e5                       ; 0xc09b9
    xor bx, bx                                ; 31 db                       ; 0xc09bb vgabios.c:168
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09bd
    mov es, ax                                ; 8e c0                       ; 0xc09c0
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c2 vgabios.c:171
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c6
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c8
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09ca
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09ce vgabios.c:175
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d4 vgabios.c:177
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09db vgabios.c:181
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e1 vgabios.c:183
    pop bp                                    ; 5d                          ; 0xc09e6 vgabios.c:184
    pop bx                                    ; 5b                          ; 0xc09e7
    retn                                      ; c3                          ; 0xc09e8
  ; disGetNextSymbol 0xc09e9 LB 0x389a -> off=0x0 cb=0000000000000022 uValue=00000000000c09e9 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09e9 LB 0x22
    inc bp                                    ; 45                          ; 0xc09e9 vgabios.c:224
    push bp                                   ; 55                          ; 0xc09ea
    mov bp, sp                                ; 89 e5                       ; 0xc09eb
    call 0099bh                               ; e8 ab ff                    ; 0xc09ed vgabios.c:226
    call 009b7h                               ; e8 c4 ff                    ; 0xc09f0 vgabios.c:227
    call 03c13h                               ; e8 1d 32                    ; 0xc09f3 vgabios.c:229
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc09f6 vgabios.c:231
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09f9
    call 00980h                               ; e8 81 ff                    ; 0xc09fc
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc09ff vgabios.c:257
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a02
    int 010h                                  ; cd 10                       ; 0xc0a04
    mov sp, bp                                ; 89 ec                       ; 0xc0a06 vgabios.c:260
    pop bp                                    ; 5d                          ; 0xc0a08
    dec bp                                    ; 4d                          ; 0xc0a09
    retf                                      ; cb                          ; 0xc0a0a
  ; disGetNextSymbol 0xc0a0b LB 0x3878 -> off=0x0 cb=0000000000000040 uValue=00000000000c0a0b 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a0b LB 0x40
    push si                                   ; 56                          ; 0xc0a0b vgabios.c:329
    push di                                   ; 57                          ; 0xc0a0c
    push bp                                   ; 55                          ; 0xc0a0d
    mov bp, sp                                ; 89 e5                       ; 0xc0a0e
    mov si, dx                                ; 89 d6                       ; 0xc0a10
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a12 vgabios.c:331
    jbe short 00a24h                          ; 76 0e                       ; 0xc0a14
    push SS                                   ; 16                          ; 0xc0a16 vgabios.c:332
    pop ES                                    ; 07                          ; 0xc0a17
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a18
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a1d vgabios.c:333
    jmp short 00a47h                          ; eb 23                       ; 0xc0a22 vgabios.c:334
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a24 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a27
    mov es, dx                                ; 8e c2                       ; 0xc0a2a
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a2c
    push SS                                   ; 16                          ; 0xc0a2f vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a30
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a31
    xor ah, ah                                ; 30 e4                       ; 0xc0a34 vgabios.c:337
    mov si, ax                                ; 89 c6                       ; 0xc0a36
    add si, ax                                ; 01 c6                       ; 0xc0a38
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a3a
    mov es, dx                                ; 8e c2                       ; 0xc0a3d vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0a3f
    push SS                                   ; 16                          ; 0xc0a42 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a43
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0a44
    pop bp                                    ; 5d                          ; 0xc0a47 vgabios.c:339
    pop di                                    ; 5f                          ; 0xc0a48
    pop si                                    ; 5e                          ; 0xc0a49
    retn                                      ; c3                          ; 0xc0a4a
  ; disGetNextSymbol 0xc0a4b LB 0x3838 -> off=0x0 cb=000000000000005e uValue=00000000000c0a4b 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a4b LB 0x5e
    push bp                                   ; 55                          ; 0xc0a4b vgabios.c:342
    mov bp, sp                                ; 89 e5                       ; 0xc0a4c
    push si                                   ; 56                          ; 0xc0a4e
    push di                                   ; 57                          ; 0xc0a4f
    push ax                                   ; 50                          ; 0xc0a50
    push ax                                   ; 50                          ; 0xc0a51
    push dx                                   ; 52                          ; 0xc0a52
    push bx                                   ; 53                          ; 0xc0a53
    mov bl, cl                                ; 88 cb                       ; 0xc0a54
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a56 vgabios.c:344
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a5b vgabios.c:346
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a5e
    je short 00a9dh                           ; 74 39                       ; 0xc0a62
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0a64 vgabios.c:347
    xor ch, ch                                ; 30 ed                       ; 0xc0a67
    mov dx, ss                                ; 8c d2                       ; 0xc0a69
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a6b
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a6e
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a71
    push DS                                   ; 1e                          ; 0xc0a74
    mov ds, dx                                ; 8e da                       ; 0xc0a75
    rep cmpsb                                 ; f3 a6                       ; 0xc0a77
    pop DS                                    ; 1f                          ; 0xc0a79
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a7a
    je short 00a81h                           ; 74 02                       ; 0xc0a7d
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a7f
    test ax, ax                               ; 85 c0                       ; 0xc0a81
    jne short 00a91h                          ; 75 0c                       ; 0xc0a83
    mov al, bl                                ; 88 d8                       ; 0xc0a85 vgabios.c:348
    xor ah, ah                                ; 30 e4                       ; 0xc0a87
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a89
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a8c
    jmp short 00a9dh                          ; eb 0c                       ; 0xc0a8f vgabios.c:349
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0a91 vgabios.c:351
    xor ah, ah                                ; 30 e4                       ; 0xc0a94
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0a96
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0a99 vgabios.c:352
    jmp short 00a5bh                          ; eb be                       ; 0xc0a9b vgabios.c:353
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0a9d vgabios.c:355
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0aa0
    pop di                                    ; 5f                          ; 0xc0aa3
    pop si                                    ; 5e                          ; 0xc0aa4
    pop bp                                    ; 5d                          ; 0xc0aa5
    retn 00004h                               ; c2 04 00                    ; 0xc0aa6
  ; disGetNextSymbol 0xc0aa9 LB 0x37da -> off=0x0 cb=0000000000000046 uValue=00000000000c0aa9 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0aa9 LB 0x46
    push bp                                   ; 55                          ; 0xc0aa9 vgabios.c:357
    mov bp, sp                                ; 89 e5                       ; 0xc0aaa
    push si                                   ; 56                          ; 0xc0aac
    push di                                   ; 57                          ; 0xc0aad
    push ax                                   ; 50                          ; 0xc0aae
    push ax                                   ; 50                          ; 0xc0aaf
    mov si, ax                                ; 89 c6                       ; 0xc0ab0
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0ab2
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0ab5
    mov bx, cx                                ; 89 cb                       ; 0xc0ab8
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0aba vgabios.c:364
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0abd
    out DX, ax                                ; ef                          ; 0xc0ac0
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0ac1 vgabios.c:366
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0ac4
    je short 00adfh                           ; 74 15                       ; 0xc0ac8
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0aca vgabios.c:367
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0acd
    not al                                    ; f6 d0                       ; 0xc0ad0
    mov di, bx                                ; 89 df                       ; 0xc0ad2
    inc bx                                    ; 43                          ; 0xc0ad4
    push SS                                   ; 16                          ; 0xc0ad5
    pop ES                                    ; 07                          ; 0xc0ad6
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ad7
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0ada vgabios.c:368
    jmp short 00ac1h                          ; eb e2                       ; 0xc0add vgabios.c:369
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0adf vgabios.c:372
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0ae2
    out DX, ax                                ; ef                          ; 0xc0ae5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ae6 vgabios.c:373
    pop di                                    ; 5f                          ; 0xc0ae9
    pop si                                    ; 5e                          ; 0xc0aea
    pop bp                                    ; 5d                          ; 0xc0aeb
    retn 00002h                               ; c2 02 00                    ; 0xc0aec
  ; disGetNextSymbol 0xc0aef LB 0x3794 -> off=0x0 cb=000000000000002f uValue=00000000000c0aef 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0aef LB 0x2f
    push si                                   ; 56                          ; 0xc0aef vgabios.c:375
    push bp                                   ; 55                          ; 0xc0af0
    mov bp, sp                                ; 89 e5                       ; 0xc0af1
    mov ch, al                                ; 88 c5                       ; 0xc0af3
    mov al, dl                                ; 88 d0                       ; 0xc0af5
    xor ah, ah                                ; 30 e4                       ; 0xc0af7 vgabios.c:379
    mul bx                                    ; f7 e3                       ; 0xc0af9
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0afb
    xor bh, bh                                ; 30 ff                       ; 0xc0afe
    mul bx                                    ; f7 e3                       ; 0xc0b00
    mov bl, ch                                ; 88 eb                       ; 0xc0b02
    add bx, ax                                ; 01 c3                       ; 0xc0b04
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b06 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b09
    mov es, ax                                ; 8e c0                       ; 0xc0b0c
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b0e
    mov al, cl                                ; 88 c8                       ; 0xc0b11 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0b13
    mul si                                    ; f7 e6                       ; 0xc0b15
    add ax, bx                                ; 01 d8                       ; 0xc0b17
    pop bp                                    ; 5d                          ; 0xc0b19 vgabios.c:383
    pop si                                    ; 5e                          ; 0xc0b1a
    retn 00002h                               ; c2 02 00                    ; 0xc0b1b
  ; disGetNextSymbol 0xc0b1e LB 0x3765 -> off=0x0 cb=0000000000000040 uValue=00000000000c0b1e 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b1e LB 0x40
    push bp                                   ; 55                          ; 0xc0b1e vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b1f
    push cx                                   ; 51                          ; 0xc0b21
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0b22
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0b25 vgabios.c:389
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0b28
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0b2c
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0b2f
    mov bx, ax                                ; 89 c3                       ; 0xc0b32
    mov ax, dx                                ; 89 d0                       ; 0xc0b34
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b36
    call 00aa9h                               ; e8 6d ff                    ; 0xc0b39
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0b3c vgabios.c:392
    push 00100h                               ; 68 00 01                    ; 0xc0b3f
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b42 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b45
    mov es, ax                                ; 8e c0                       ; 0xc0b47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b49
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b4c
    xor cx, cx                                ; 31 c9                       ; 0xc0b50 vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0b52
    call 00a4bh                               ; e8 f3 fe                    ; 0xc0b55
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0b58 vgabios.c:393
    pop cx                                    ; 59                          ; 0xc0b5b
    pop bp                                    ; 5d                          ; 0xc0b5c
    retn                                      ; c3                          ; 0xc0b5d
  ; disGetNextSymbol 0xc0b5e LB 0x3725 -> off=0x0 cb=0000000000000024 uValue=00000000000c0b5e 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b5e LB 0x24
    enter 00002h, 000h                        ; c8 02 00 00                 ; 0xc0b5e vgabios.c:395
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0b62
    mov al, dl                                ; 88 d0                       ; 0xc0b65 vgabios.c:399
    xor ah, ah                                ; 30 e4                       ; 0xc0b67
    mul bx                                    ; f7 e3                       ; 0xc0b69
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0b6b
    xor dh, dh                                ; 30 f6                       ; 0xc0b6e
    mul dx                                    ; f7 e2                       ; 0xc0b70
    mov dx, ax                                ; 89 c2                       ; 0xc0b72
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0b74
    xor ah, ah                                ; 30 e4                       ; 0xc0b77
    add ax, dx                                ; 01 d0                       ; 0xc0b79
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0b7b vgabios.c:400
    leave                                     ; c9                          ; 0xc0b7e vgabios.c:402
    retn 00002h                               ; c2 02 00                    ; 0xc0b7f
  ; disGetNextSymbol 0xc0b82 LB 0x3701 -> off=0x0 cb=000000000000004b uValue=00000000000c0b82 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b82 LB 0x4b
    push si                                   ; 56                          ; 0xc0b82 vgabios.c:404
    push di                                   ; 57                          ; 0xc0b83
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0b84
    mov si, ax                                ; 89 c6                       ; 0xc0b88
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0b8a
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0b8d
    mov bx, cx                                ; 89 cb                       ; 0xc0b90
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0b92 vgabios.c:410
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0b95
    je short 00bc7h                           ; 74 2c                       ; 0xc0b99
    xor dh, dh                                ; 30 f6                       ; 0xc0b9b vgabios.c:411
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0b9d vgabios.c:412
    xor ax, ax                                ; 31 c0                       ; 0xc0b9f vgabios.c:413
    jmp short 00ba8h                          ; eb 05                       ; 0xc0ba1
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0ba3
    jnl short 00bbch                          ; 7d 14                       ; 0xc0ba6
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0ba8 vgabios.c:414
    mov di, si                                ; 89 f7                       ; 0xc0bab
    add di, ax                                ; 01 c7                       ; 0xc0bad
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0baf
    je short 00bb7h                           ; 74 02                       ; 0xc0bb3
    or dh, dl                                 ; 08 d6                       ; 0xc0bb5 vgabios.c:415
    shr dl, 1                                 ; d0 ea                       ; 0xc0bb7 vgabios.c:416
    inc ax                                    ; 40                          ; 0xc0bb9 vgabios.c:417
    jmp short 00ba3h                          ; eb e7                       ; 0xc0bba
    mov di, bx                                ; 89 df                       ; 0xc0bbc vgabios.c:418
    inc bx                                    ; 43                          ; 0xc0bbe
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0bbf
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bc2 vgabios.c:419
    jmp short 00b92h                          ; eb cb                       ; 0xc0bc5 vgabios.c:420
    leave                                     ; c9                          ; 0xc0bc7 vgabios.c:421
    pop di                                    ; 5f                          ; 0xc0bc8
    pop si                                    ; 5e                          ; 0xc0bc9
    retn 00002h                               ; c2 02 00                    ; 0xc0bca
  ; disGetNextSymbol 0xc0bcd LB 0x36b6 -> off=0x0 cb=0000000000000045 uValue=00000000000c0bcd 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0bcd LB 0x45
    push bp                                   ; 55                          ; 0xc0bcd vgabios.c:423
    mov bp, sp                                ; 89 e5                       ; 0xc0bce
    push cx                                   ; 51                          ; 0xc0bd0
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0bd1
    mov cx, ax                                ; 89 c1                       ; 0xc0bd4
    mov ax, dx                                ; 89 d0                       ; 0xc0bd6
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0bd8 vgabios.c:427
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0bdb
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bdf
    mov bx, cx                                ; 89 cb                       ; 0xc0be2
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0be4
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0be7
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bea
    call 00b82h                               ; e8 92 ff                    ; 0xc0bed
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bf0 vgabios.c:430
    push 00100h                               ; 68 00 01                    ; 0xc0bf3
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0bf6 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0bf9
    mov es, ax                                ; 8e c0                       ; 0xc0bfb
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0bfd
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c00
    xor cx, cx                                ; 31 c9                       ; 0xc0c04 vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0c06
    call 00a4bh                               ; e8 3f fe                    ; 0xc0c09
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0c0c vgabios.c:431
    pop cx                                    ; 59                          ; 0xc0c0f
    pop bp                                    ; 5d                          ; 0xc0c10
    retn                                      ; c3                          ; 0xc0c11
  ; disGetNextSymbol 0xc0c12 LB 0x3671 -> off=0x0 cb=0000000000000035 uValue=00000000000c0c12 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c12 LB 0x35
    push bp                                   ; 55                          ; 0xc0c12 vgabios.c:433
    mov bp, sp                                ; 89 e5                       ; 0xc0c13
    push bx                                   ; 53                          ; 0xc0c15
    push cx                                   ; 51                          ; 0xc0c16
    mov bx, ax                                ; 89 c3                       ; 0xc0c17
    mov es, dx                                ; 8e c2                       ; 0xc0c19
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c1b vgabios.c:439
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c1e vgabios.c:440
    xor dl, dl                                ; 30 d2                       ; 0xc0c20 vgabios.c:441
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c22 vgabios.c:442
    xchg ah, al                               ; 86 c4                       ; 0xc0c25
    xor bx, bx                                ; 31 db                       ; 0xc0c27 vgabios.c:444
    jmp short 00c30h                          ; eb 05                       ; 0xc0c29
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c2b
    jnl short 00c3eh                          ; 7d 0e                       ; 0xc0c2e
    test ax, cx                               ; 85 c8                       ; 0xc0c30 vgabios.c:445
    je short 00c36h                           ; 74 02                       ; 0xc0c32
    or dl, dh                                 ; 08 f2                       ; 0xc0c34 vgabios.c:446
    shr dh, 1                                 ; d0 ee                       ; 0xc0c36 vgabios.c:447
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0c38 vgabios.c:448
    inc bx                                    ; 43                          ; 0xc0c3b vgabios.c:449
    jmp short 00c2bh                          ; eb ed                       ; 0xc0c3c
    mov al, dl                                ; 88 d0                       ; 0xc0c3e vgabios.c:451
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c40
    pop cx                                    ; 59                          ; 0xc0c43
    pop bx                                    ; 5b                          ; 0xc0c44
    pop bp                                    ; 5d                          ; 0xc0c45
    retn                                      ; c3                          ; 0xc0c46
  ; disGetNextSymbol 0xc0c47 LB 0x363c -> off=0x0 cb=0000000000000084 uValue=00000000000c0c47 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c47 LB 0x84
    push bp                                   ; 55                          ; 0xc0c47 vgabios.c:453
    mov bp, sp                                ; 89 e5                       ; 0xc0c48
    push cx                                   ; 51                          ; 0xc0c4a
    push si                                   ; 56                          ; 0xc0c4b
    push di                                   ; 57                          ; 0xc0c4c
    push ax                                   ; 50                          ; 0xc0c4d
    mov si, dx                                ; 89 d6                       ; 0xc0c4e
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c50 vgabios.c:461
    je short 00c8fh                           ; 74 3a                       ; 0xc0c53
    mov bx, ax                                ; 89 c3                       ; 0xc0c55 vgabios.c:463
    add bx, ax                                ; 01 c3                       ; 0xc0c57
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c59
    xor cx, cx                                ; 31 c9                       ; 0xc0c5e vgabios.c:465
    jmp short 00c67h                          ; eb 05                       ; 0xc0c60
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c62
    jnl short 00cc3h                          ; 7d 5c                       ; 0xc0c65
    mov ax, bx                                ; 89 d8                       ; 0xc0c67 vgabios.c:466
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c69
    call 00c12h                               ; e8 a3 ff                    ; 0xc0c6c
    mov di, si                                ; 89 f7                       ; 0xc0c6f
    inc si                                    ; 46                          ; 0xc0c71
    push SS                                   ; 16                          ; 0xc0c72
    pop ES                                    ; 07                          ; 0xc0c73
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c74
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c77 vgabios.c:467
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c7b
    call 00c12h                               ; e8 91 ff                    ; 0xc0c7e
    mov di, si                                ; 89 f7                       ; 0xc0c81
    inc si                                    ; 46                          ; 0xc0c83
    push SS                                   ; 16                          ; 0xc0c84
    pop ES                                    ; 07                          ; 0xc0c85
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c86
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0c89 vgabios.c:468
    inc cx                                    ; 41                          ; 0xc0c8c vgabios.c:469
    jmp short 00c62h                          ; eb d3                       ; 0xc0c8d
    mov bx, ax                                ; 89 c3                       ; 0xc0c8f vgabios.c:471
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c91
    xor cx, cx                                ; 31 c9                       ; 0xc0c96 vgabios.c:472
    jmp short 00c9fh                          ; eb 05                       ; 0xc0c98
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c9a
    jnl short 00cc3h                          ; 7d 24                       ; 0xc0c9d
    mov di, si                                ; 89 f7                       ; 0xc0c9f vgabios.c:473
    inc si                                    ; 46                          ; 0xc0ca1
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0ca2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ca5
    push SS                                   ; 16                          ; 0xc0ca8
    pop ES                                    ; 07                          ; 0xc0ca9
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0caa
    mov di, si                                ; 89 f7                       ; 0xc0cad vgabios.c:474
    inc si                                    ; 46                          ; 0xc0caf
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cb0
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0cb3
    push SS                                   ; 16                          ; 0xc0cb8
    pop ES                                    ; 07                          ; 0xc0cb9
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cba
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0cbd vgabios.c:475
    inc cx                                    ; 41                          ; 0xc0cc0 vgabios.c:476
    jmp short 00c9ah                          ; eb d7                       ; 0xc0cc1
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0cc3 vgabios.c:478
    pop di                                    ; 5f                          ; 0xc0cc6
    pop si                                    ; 5e                          ; 0xc0cc7
    pop cx                                    ; 59                          ; 0xc0cc8
    pop bp                                    ; 5d                          ; 0xc0cc9
    retn                                      ; c3                          ; 0xc0cca
  ; disGetNextSymbol 0xc0ccb LB 0x35b8 -> off=0x0 cb=000000000000001a uValue=00000000000c0ccb 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0ccb LB 0x1a
    push cx                                   ; 51                          ; 0xc0ccb vgabios.c:480
    push bp                                   ; 55                          ; 0xc0ccc
    mov bp, sp                                ; 89 e5                       ; 0xc0ccd
    mov cl, al                                ; 88 c1                       ; 0xc0ccf
    mov al, dl                                ; 88 d0                       ; 0xc0cd1
    xor ah, ah                                ; 30 e4                       ; 0xc0cd3 vgabios.c:485
    mul bx                                    ; f7 e3                       ; 0xc0cd5
    mov bx, ax                                ; 89 c3                       ; 0xc0cd7
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0cd9
    mov al, cl                                ; 88 c8                       ; 0xc0cdc
    xor ah, ah                                ; 30 e4                       ; 0xc0cde
    add ax, bx                                ; 01 d8                       ; 0xc0ce0
    pop bp                                    ; 5d                          ; 0xc0ce2 vgabios.c:486
    pop cx                                    ; 59                          ; 0xc0ce3
    retn                                      ; c3                          ; 0xc0ce4
  ; disGetNextSymbol 0xc0ce5 LB 0x359e -> off=0x0 cb=0000000000000066 uValue=00000000000c0ce5 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0ce5 LB 0x66
    push bp                                   ; 55                          ; 0xc0ce5 vgabios.c:488
    mov bp, sp                                ; 89 e5                       ; 0xc0ce6
    push bx                                   ; 53                          ; 0xc0ce8
    push cx                                   ; 51                          ; 0xc0ce9
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0cea
    mov bl, dl                                ; 88 d3                       ; 0xc0ced vgabios.c:494
    xor bh, bh                                ; 30 ff                       ; 0xc0cef
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0cf1
    call 00c47h                               ; e8 50 ff                    ; 0xc0cf4
    push strict byte 00008h                   ; 6a 08                       ; 0xc0cf7 vgabios.c:497
    push 00080h                               ; 68 80 00                    ; 0xc0cf9
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0cfc vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0cff
    mov es, ax                                ; 8e c0                       ; 0xc0d01
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d03
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d06
    xor cx, cx                                ; 31 c9                       ; 0xc0d0a vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d0c
    call 00a4bh                               ; e8 39 fd                    ; 0xc0d0f
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d12
    test ah, 080h                             ; f6 c4 80                    ; 0xc0d15 vgabios.c:499
    jne short 00d41h                          ; 75 27                       ; 0xc0d18
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d1a vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d1d
    mov es, ax                                ; 8e c0                       ; 0xc0d1f
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d21
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d24
    test dx, dx                               ; 85 d2                       ; 0xc0d28 vgabios.c:503
    jne short 00d30h                          ; 75 04                       ; 0xc0d2a
    test ax, ax                               ; 85 c0                       ; 0xc0d2c
    je short 00d41h                           ; 74 11                       ; 0xc0d2e
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d30 vgabios.c:504
    push 00080h                               ; 68 80 00                    ; 0xc0d32
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0d35
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d38
    call 00a4bh                               ; e8 0d fd                    ; 0xc0d3b
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d3e
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d41 vgabios.c:507
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d44
    pop cx                                    ; 59                          ; 0xc0d47
    pop bx                                    ; 5b                          ; 0xc0d48
    pop bp                                    ; 5d                          ; 0xc0d49
    retn                                      ; c3                          ; 0xc0d4a
  ; disGetNextSymbol 0xc0d4b LB 0x3538 -> off=0x0 cb=0000000000000130 uValue=00000000000c0d4b 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d4b LB 0x130
    push bp                                   ; 55                          ; 0xc0d4b vgabios.c:509
    mov bp, sp                                ; 89 e5                       ; 0xc0d4c
    push bx                                   ; 53                          ; 0xc0d4e
    push cx                                   ; 51                          ; 0xc0d4f
    push si                                   ; 56                          ; 0xc0d50
    push di                                   ; 57                          ; 0xc0d51
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc0d52
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0d55
    mov si, dx                                ; 89 d6                       ; 0xc0d58
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d5a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d5d
    mov es, ax                                ; 8e c0                       ; 0xc0d60
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d62
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0d65 vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d68 vgabios.c:517
    call 035d1h                               ; e8 64 28                    ; 0xc0d6a
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d6d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d70 vgabios.c:518
    jne short 00d77h                          ; 75 03                       ; 0xc0d72
    jmp near 00e72h                           ; e9 fb 00                    ; 0xc0d74
    mov cl, byte [bp-00eh]                    ; 8a 4e f2                    ; 0xc0d77 vgabios.c:522
    xor ch, ch                                ; 30 ed                       ; 0xc0d7a
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc0d7c
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0d7f
    mov ax, cx                                ; 89 c8                       ; 0xc0d82
    call 00a0bh                               ; e8 84 fc                    ; 0xc0d84
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc0d87 vgabios.c:523
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d8a
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc0d8d vgabios.c:524
    xor al, al                                ; 30 c0                       ; 0xc0d90
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0d92
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc0d95
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc0d98
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0d9b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d9e
    mov es, ax                                ; 8e c0                       ; 0xc0da1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0da3
    xor ah, ah                                ; 30 e4                       ; 0xc0da6 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc0da8
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc0da9
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0dac vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0daf
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc0db2 vgabios.c:48
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc0db5 vgabios.c:530
    xor bh, bh                                ; 30 ff                       ; 0xc0db8
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0dba
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0dbd
    jne short 00df4h                          ; 75 30                       ; 0xc0dc2
    mov ax, di                                ; 89 f8                       ; 0xc0dc4 vgabios.c:532
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc0dc6
    add ax, ax                                ; 01 c0                       ; 0xc0dc9
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0dcb
    inc ax                                    ; 40                          ; 0xc0dcd
    mul cx                                    ; f7 e1                       ; 0xc0dce
    mov cx, ax                                ; 89 c1                       ; 0xc0dd0
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc0dd2
    xor ah, ah                                ; 30 e4                       ; 0xc0dd5
    mul di                                    ; f7 e7                       ; 0xc0dd7
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0dd9
    xor dh, dh                                ; 30 f6                       ; 0xc0ddc
    mov di, ax                                ; 89 c7                       ; 0xc0dde
    add di, dx                                ; 01 d7                       ; 0xc0de0
    add di, di                                ; 01 ff                       ; 0xc0de2
    add di, cx                                ; 01 cf                       ; 0xc0de4
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0de6 vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0dea
    push SS                                   ; 16                          ; 0xc0ded vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0dee
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0def
    jmp short 00d74h                          ; eb 80                       ; 0xc0df2 vgabios.c:534
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0df4 vgabios.c:535
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0df8
    je short 00e4bh                           ; 74 4e                       ; 0xc0dfb
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0dfd
    jc short 00e72h                           ; 72 70                       ; 0xc0e00
    jbe short 00e0bh                          ; 76 07                       ; 0xc0e02
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e04
    jbe short 00e24h                          ; 76 1b                       ; 0xc0e07
    jmp short 00e72h                          ; eb 67                       ; 0xc0e09
    xor dh, dh                                ; 30 f6                       ; 0xc0e0b vgabios.c:538
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e0d
    xor ah, ah                                ; 30 e4                       ; 0xc0e10
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc0e12
    call 00ccbh                               ; e8 b3 fe                    ; 0xc0e15
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc0e18 vgabios.c:539
    xor dh, dh                                ; 30 f6                       ; 0xc0e1b
    call 00ce5h                               ; e8 c5 fe                    ; 0xc0e1d
    xor ah, ah                                ; 30 e4                       ; 0xc0e20
    jmp short 00dedh                          ; eb c9                       ; 0xc0e22
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e24 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e27
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0e2a vgabios.c:544
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0e2d
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0e30
    xor dh, dh                                ; 30 f6                       ; 0xc0e33
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e35
    xor ah, ah                                ; 30 e4                       ; 0xc0e38
    mov bx, di                                ; 89 fb                       ; 0xc0e3a
    call 00aefh                               ; e8 b0 fc                    ; 0xc0e3c
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0e3f vgabios.c:545
    mov dx, ax                                ; 89 c2                       ; 0xc0e42
    mov ax, di                                ; 89 f8                       ; 0xc0e44
    call 00b1eh                               ; e8 d5 fc                    ; 0xc0e46
    jmp short 00e20h                          ; eb d5                       ; 0xc0e49
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e4b vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e4e
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0e51 vgabios.c:549
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0e54
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0e57
    xor dh, dh                                ; 30 f6                       ; 0xc0e5a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e5c
    xor ah, ah                                ; 30 e4                       ; 0xc0e5f
    mov bx, di                                ; 89 fb                       ; 0xc0e61
    call 00b5eh                               ; e8 f8 fc                    ; 0xc0e63
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0e66 vgabios.c:550
    mov dx, ax                                ; 89 c2                       ; 0xc0e69
    mov ax, di                                ; 89 f8                       ; 0xc0e6b
    call 00bcdh                               ; e8 5d fd                    ; 0xc0e6d
    jmp short 00e20h                          ; eb ae                       ; 0xc0e70
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e72 vgabios.c:559
    pop di                                    ; 5f                          ; 0xc0e75
    pop si                                    ; 5e                          ; 0xc0e76
    pop cx                                    ; 59                          ; 0xc0e77
    pop bx                                    ; 5b                          ; 0xc0e78
    pop bp                                    ; 5d                          ; 0xc0e79
    retn                                      ; c3                          ; 0xc0e7a
  ; disGetNextSymbol 0xc0e7b LB 0x3408 -> off=0x10 cb=0000000000000089 uValue=00000000000c0e8b 'vga_get_font_info'
    db  0a6h, 00eh, 0ech, 00eh, 0f1h, 00eh, 0f9h, 00eh, 0feh, 00eh, 003h, 00fh, 008h, 00fh, 00dh, 00fh
vga_get_font_info:                           ; 0xc0e8b LB 0x89
    push si                                   ; 56                          ; 0xc0e8b vgabios.c:561
    push di                                   ; 57                          ; 0xc0e8c
    enter 00002h, 000h                        ; c8 02 00 00                 ; 0xc0e8d
    mov di, dx                                ; 89 d7                       ; 0xc0e91
    mov word [bp-002h], bx                    ; 89 5e fe                    ; 0xc0e93
    mov si, cx                                ; 89 ce                       ; 0xc0e96
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0e98 vgabios.c:566
    jnbe short 00ee6h                         ; 77 49                       ; 0xc0e9b
    mov bx, ax                                ; 89 c3                       ; 0xc0e9d
    add bx, ax                                ; 01 c3                       ; 0xc0e9f
    jmp word [cs:bx+00e7bh]                   ; 2e ff a7 7b 0e              ; 0xc0ea1
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0ea6 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0ea9
    mov es, ax                                ; 8e c0                       ; 0xc0eab
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0ead
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0eb0
    push SS                                   ; 16                          ; 0xc0eb4 vgabios.c:569
    pop ES                                    ; 07                          ; 0xc0eb5
    mov bx, word [bp-002h]                    ; 8b 5e fe                    ; 0xc0eb6
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0eb9
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0ebc
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0ebf
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ec2
    mov es, ax                                ; 8e c0                       ; 0xc0ec5
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ec7
    xor ah, ah                                ; 30 e4                       ; 0xc0eca
    push SS                                   ; 16                          ; 0xc0ecc
    pop ES                                    ; 07                          ; 0xc0ecd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0ece
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0ed1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ed4
    mov es, ax                                ; 8e c0                       ; 0xc0ed7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ed9
    xor ah, ah                                ; 30 e4                       ; 0xc0edc
    push SS                                   ; 16                          ; 0xc0ede
    pop ES                                    ; 07                          ; 0xc0edf
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0ee0
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ee3
    leave                                     ; c9                          ; 0xc0ee6
    pop di                                    ; 5f                          ; 0xc0ee7
    pop si                                    ; 5e                          ; 0xc0ee8
    retn 00002h                               ; c2 02 00                    ; 0xc0ee9
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0eec vgabios.c:57
    jmp short 00ea9h                          ; eb b8                       ; 0xc0eef
    mov ax, 05d6ch                            ; b8 6c 5d                    ; 0xc0ef1 vgabios.c:574
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc0ef4
    jmp short 00eb4h                          ; eb bb                       ; 0xc0ef7 vgabios.c:575
    mov ax, 0556ch                            ; b8 6c 55                    ; 0xc0ef9 vgabios.c:577
    jmp short 00ef4h                          ; eb f6                       ; 0xc0efc
    mov ax, 0596ch                            ; b8 6c 59                    ; 0xc0efe vgabios.c:580
    jmp short 00ef4h                          ; eb f1                       ; 0xc0f01
    mov ax, 07b6ch                            ; b8 6c 7b                    ; 0xc0f03 vgabios.c:583
    jmp short 00ef4h                          ; eb ec                       ; 0xc0f06
    mov ax, 06b6ch                            ; b8 6c 6b                    ; 0xc0f08 vgabios.c:586
    jmp short 00ef4h                          ; eb e7                       ; 0xc0f0b
    mov ax, 07c99h                            ; b8 99 7c                    ; 0xc0f0d vgabios.c:589
    jmp short 00ef4h                          ; eb e2                       ; 0xc0f10
    jmp short 00ee6h                          ; eb d2                       ; 0xc0f12 vgabios.c:595
  ; disGetNextSymbol 0xc0f14 LB 0x336f -> off=0x0 cb=0000000000000166 uValue=00000000000c0f14 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f14 LB 0x166
    push bp                                   ; 55                          ; 0xc0f14 vgabios.c:608
    mov bp, sp                                ; 89 e5                       ; 0xc0f15
    push si                                   ; 56                          ; 0xc0f17
    push di                                   ; 57                          ; 0xc0f18
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0f19
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0f1c
    mov si, dx                                ; 89 d6                       ; 0xc0f1f
    mov dx, bx                                ; 89 da                       ; 0xc0f21
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc0f23
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0f26 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f29
    mov es, ax                                ; 8e c0                       ; 0xc0f2c
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f2e
    xor ah, ah                                ; 30 e4                       ; 0xc0f31 vgabios.c:615
    call 035d1h                               ; e8 9b 26                    ; 0xc0f33
    mov ah, al                                ; 88 c4                       ; 0xc0f36
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f38 vgabios.c:616
    je short 00f4ah                           ; 74 0e                       ; 0xc0f3a
    mov bl, al                                ; 88 c3                       ; 0xc0f3c vgabios.c:618
    xor bh, bh                                ; 30 ff                       ; 0xc0f3e
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0f40
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f43
    jne short 00f4dh                          ; 75 03                       ; 0xc0f48
    jmp near 01073h                           ; e9 26 01                    ; 0xc0f4a vgabios.c:619
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f4d vgabios.c:622
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0f51
    jc short 00f65h                           ; 72 0f                       ; 0xc0f54
    jbe short 00f6dh                          ; 76 15                       ; 0xc0f56
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f58
    je short 00fa4h                           ; 74 47                       ; 0xc0f5b
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f5d
    je short 00f6dh                           ; 74 0b                       ; 0xc0f60
    jmp near 01069h                           ; e9 04 01                    ; 0xc0f62
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f65
    je short 00fdbh                           ; 74 71                       ; 0xc0f68
    jmp near 01069h                           ; e9 fc 00                    ; 0xc0f6a
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0f6d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f70
    mov es, ax                                ; 8e c0                       ; 0xc0f73
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc0f75
    mov ax, dx                                ; 89 d0                       ; 0xc0f78 vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc0f7a
    mov bx, si                                ; 89 f3                       ; 0xc0f7c
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0f7e
    add bx, ax                                ; 01 c3                       ; 0xc0f81
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0f83 vgabios.c:47
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0f86
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0f89 vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc0f8c
    mul dx                                    ; f7 e2                       ; 0xc0f8e
    add bx, ax                                ; 01 c3                       ; 0xc0f90
    mov cx, si                                ; 89 f1                       ; 0xc0f92 vgabios.c:627
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0f94
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0f97
    sar ax, CL                                ; d3 f8                       ; 0xc0f9a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0f9c
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0f9f vgabios.c:629
    jmp short 00fadh                          ; eb 09                       ; 0xc0fa2
    jmp near 01049h                           ; e9 a2 00                    ; 0xc0fa4
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0fa7
    jnc short 00fd8h                          ; 73 2b                       ; 0xc0fab
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0fad vgabios.c:630
    xor ah, ah                                ; 30 e4                       ; 0xc0fb0
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0fb2
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0fb5
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0fb7
    out DX, ax                                ; ef                          ; 0xc0fba
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0fbb vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0fbe
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fc0
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0fc3 vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0fc6 vgabios.c:632
    jbe short 00fd3h                          ; 76 09                       ; 0xc0fc8
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0fca vgabios.c:633
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0fcd
    sal al, CL                                ; d2 e0                       ; 0xc0fcf
    or ch, al                                 ; 08 c5                       ; 0xc0fd1
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0fd3 vgabios.c:634
    jmp short 00fa7h                          ; eb cf                       ; 0xc0fd6
    jmp near 0106bh                           ; e9 90 00                    ; 0xc0fd8
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc0fdb vgabios.c:637
    xor ch, ch                                ; 30 ed                       ; 0xc0fdf
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc0fe1
    sub bx, cx                                ; 29 cb                       ; 0xc0fe4
    mov cx, bx                                ; 89 d9                       ; 0xc0fe6
    mov bx, si                                ; 89 f3                       ; 0xc0fe8
    shr bx, CL                                ; d3 eb                       ; 0xc0fea
    mov cx, bx                                ; 89 d9                       ; 0xc0fec
    mov bx, dx                                ; 89 d3                       ; 0xc0fee
    shr bx, 1                                 ; d1 eb                       ; 0xc0ff0
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc0ff2
    add bx, cx                                ; 01 cb                       ; 0xc0ff5
    test dl, 001h                             ; f6 c2 01                    ; 0xc0ff7 vgabios.c:638
    je short 00fffh                           ; 74 03                       ; 0xc0ffa
    add bh, 020h                              ; 80 c7 20                    ; 0xc0ffc vgabios.c:639
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc0fff vgabios.c:37
    mov es, dx                                ; 8e c2                       ; 0xc1002
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1004
    mov bl, ah                                ; 88 e3                       ; 0xc1007 vgabios.c:641
    xor bh, bh                                ; 30 ff                       ; 0xc1009
    sal bx, 003h                              ; c1 e3 03                    ; 0xc100b
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc100e
    jne short 01030h                          ; 75 1b                       ; 0xc1013
    mov cx, si                                ; 89 f1                       ; 0xc1015 vgabios.c:642
    xor ch, ch                                ; 30 ed                       ; 0xc1017
    and cl, 003h                              ; 80 e1 03                    ; 0xc1019
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc101c
    sub dx, cx                                ; 29 ca                       ; 0xc101f
    mov cx, dx                                ; 89 d1                       ; 0xc1021
    add cx, dx                                ; 01 d1                       ; 0xc1023
    xor ah, ah                                ; 30 e4                       ; 0xc1025
    sar ax, CL                                ; d3 f8                       ; 0xc1027
    mov ch, al                                ; 88 c5                       ; 0xc1029
    and ch, 003h                              ; 80 e5 03                    ; 0xc102b
    jmp short 0106bh                          ; eb 3b                       ; 0xc102e vgabios.c:643
    mov cx, si                                ; 89 f1                       ; 0xc1030 vgabios.c:644
    xor ch, ch                                ; 30 ed                       ; 0xc1032
    and cl, 007h                              ; 80 e1 07                    ; 0xc1034
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc1037
    sub dx, cx                                ; 29 ca                       ; 0xc103a
    mov cx, dx                                ; 89 d1                       ; 0xc103c
    xor ah, ah                                ; 30 e4                       ; 0xc103e
    sar ax, CL                                ; d3 f8                       ; 0xc1040
    mov ch, al                                ; 88 c5                       ; 0xc1042
    and ch, 001h                              ; 80 e5 01                    ; 0xc1044
    jmp short 0106bh                          ; eb 22                       ; 0xc1047 vgabios.c:645
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1049 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc104c
    mov es, ax                                ; 8e c0                       ; 0xc104f
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1051
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1054 vgabios.c:48
    mov ax, dx                                ; 89 d0                       ; 0xc1057
    mul bx                                    ; f7 e3                       ; 0xc1059
    mov bx, si                                ; 89 f3                       ; 0xc105b
    add bx, ax                                ; 01 c3                       ; 0xc105d
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc105f vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc1062
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc1064
    jmp short 0106bh                          ; eb 02                       ; 0xc1067 vgabios.c:649
    xor ch, ch                                ; 30 ed                       ; 0xc1069 vgabios.c:654
    push SS                                   ; 16                          ; 0xc106b vgabios.c:656
    pop ES                                    ; 07                          ; 0xc106c
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc106d
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc1070
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1073 vgabios.c:657
    pop di                                    ; 5f                          ; 0xc1076
    pop si                                    ; 5e                          ; 0xc1077
    pop bp                                    ; 5d                          ; 0xc1078
    retn                                      ; c3                          ; 0xc1079
  ; disGetNextSymbol 0xc107a LB 0x3209 -> off=0x0 cb=000000000000008d uValue=00000000000c107a 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc107a LB 0x8d
    push bp                                   ; 55                          ; 0xc107a vgabios.c:662
    mov bp, sp                                ; 89 e5                       ; 0xc107b
    push bx                                   ; 53                          ; 0xc107d
    push cx                                   ; 51                          ; 0xc107e
    push si                                   ; 56                          ; 0xc107f
    push di                                   ; 57                          ; 0xc1080
    push ax                                   ; 50                          ; 0xc1081
    push ax                                   ; 50                          ; 0xc1082
    mov bx, ax                                ; 89 c3                       ; 0xc1083
    mov di, dx                                ; 89 d7                       ; 0xc1085
    mov dx, 003dah                            ; ba da 03                    ; 0xc1087 vgabios.c:667
    in AL, DX                                 ; ec                          ; 0xc108a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc108b
    xor al, al                                ; 30 c0                       ; 0xc108d vgabios.c:668
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc108f
    out DX, AL                                ; ee                          ; 0xc1092
    xor si, si                                ; 31 f6                       ; 0xc1093 vgabios.c:670
    cmp si, di                                ; 39 fe                       ; 0xc1095
    jnc short 010ech                          ; 73 53                       ; 0xc1097
    mov al, bl                                ; 88 d8                       ; 0xc1099 vgabios.c:673
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc109b
    out DX, AL                                ; ee                          ; 0xc109e
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc109f vgabios.c:675
    in AL, DX                                 ; ec                          ; 0xc10a2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10a3
    mov cx, ax                                ; 89 c1                       ; 0xc10a5
    in AL, DX                                 ; ec                          ; 0xc10a7 vgabios.c:676
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10a8
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc10aa
    in AL, DX                                 ; ec                          ; 0xc10ad vgabios.c:677
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10ae
    xor ch, ch                                ; 30 ed                       ; 0xc10b0 vgabios.c:680
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc10b2
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc10b5
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc10b8
    xor ch, ch                                ; 30 ed                       ; 0xc10bb
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc10bd
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc10c1
    xor ah, ah                                ; 30 e4                       ; 0xc10c4
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc10c6
    add cx, ax                                ; 01 c1                       ; 0xc10c9
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc10cb
    sar cx, 008h                              ; c1 f9 08                    ; 0xc10cf
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc10d2 vgabios.c:682
    jbe short 010dah                          ; 76 03                       ; 0xc10d5
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc10d7
    mov al, bl                                ; 88 d8                       ; 0xc10da vgabios.c:685
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc10dc
    out DX, AL                                ; ee                          ; 0xc10df
    mov al, cl                                ; 88 c8                       ; 0xc10e0 vgabios.c:687
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10e2
    out DX, AL                                ; ee                          ; 0xc10e5
    out DX, AL                                ; ee                          ; 0xc10e6 vgabios.c:688
    out DX, AL                                ; ee                          ; 0xc10e7 vgabios.c:689
    inc bx                                    ; 43                          ; 0xc10e8 vgabios.c:690
    inc si                                    ; 46                          ; 0xc10e9 vgabios.c:691
    jmp short 01095h                          ; eb a9                       ; 0xc10ea
    mov dx, 003dah                            ; ba da 03                    ; 0xc10ec vgabios.c:692
    in AL, DX                                 ; ec                          ; 0xc10ef
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10f0
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc10f2 vgabios.c:693
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10f4
    out DX, AL                                ; ee                          ; 0xc10f7
    mov dx, 003dah                            ; ba da 03                    ; 0xc10f8 vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc10fb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10fc
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc10fe vgabios.c:697
    pop di                                    ; 5f                          ; 0xc1101
    pop si                                    ; 5e                          ; 0xc1102
    pop cx                                    ; 59                          ; 0xc1103
    pop bx                                    ; 5b                          ; 0xc1104
    pop bp                                    ; 5d                          ; 0xc1105
    retn                                      ; c3                          ; 0xc1106
  ; disGetNextSymbol 0xc1107 LB 0x317c -> off=0x0 cb=0000000000000107 uValue=00000000000c1107 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc1107 LB 0x107
    push bp                                   ; 55                          ; 0xc1107 vgabios.c:700
    mov bp, sp                                ; 89 e5                       ; 0xc1108
    push bx                                   ; 53                          ; 0xc110a
    push cx                                   ; 51                          ; 0xc110b
    push si                                   ; 56                          ; 0xc110c
    push ax                                   ; 50                          ; 0xc110d
    push ax                                   ; 50                          ; 0xc110e
    mov bl, al                                ; 88 c3                       ; 0xc110f
    mov ah, dl                                ; 88 d4                       ; 0xc1111
    mov dl, al                                ; 88 c2                       ; 0xc1113 vgabios.c:706
    xor dh, dh                                ; 30 f6                       ; 0xc1115
    mov cx, dx                                ; 89 d1                       ; 0xc1117
    sal cx, 008h                              ; c1 e1 08                    ; 0xc1119
    mov dl, ah                                ; 88 e2                       ; 0xc111c
    add dx, cx                                ; 01 ca                       ; 0xc111e
    mov si, strict word 00060h                ; be 60 00                    ; 0xc1120 vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc1123
    mov es, cx                                ; 8e c1                       ; 0xc1126
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc1128
    mov si, 00087h                            ; be 87 00                    ; 0xc112b vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc112e
    test dl, 008h                             ; f6 c2 08                    ; 0xc1131 vgabios.c:38
    jne short 01173h                          ; 75 3d                       ; 0xc1134
    mov dl, al                                ; 88 c2                       ; 0xc1136 vgabios.c:712
    and dl, 060h                              ; 80 e2 60                    ; 0xc1138
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc113b
    jne short 01146h                          ; 75 06                       ; 0xc113e
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc1140 vgabios.c:714
    xor ah, ah                                ; 30 e4                       ; 0xc1142 vgabios.c:715
    jmp short 01173h                          ; eb 2d                       ; 0xc1144 vgabios.c:716
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1146 vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc1149 vgabios.c:38
    jne short 011a8h                          ; 75 5a                       ; 0xc114c
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc114e
    jnc short 011a8h                          ; 73 55                       ; 0xc1151
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc1153
    jnc short 011a8h                          ; 73 50                       ; 0xc1156
    mov si, 00085h                            ; be 85 00                    ; 0xc1158 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc115b
    mov es, dx                                ; 8e c2                       ; 0xc115e
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1160
    mov dx, cx                                ; 89 ca                       ; 0xc1163 vgabios.c:48
    cmp ah, bl                                ; 38 dc                       ; 0xc1165 vgabios.c:727
    jnc short 01175h                          ; 73 0c                       ; 0xc1167
    test ah, ah                               ; 84 e4                       ; 0xc1169 vgabios.c:729
    je short 011a8h                           ; 74 3b                       ; 0xc116b
    xor bl, bl                                ; 30 db                       ; 0xc116d vgabios.c:730
    mov ah, cl                                ; 88 cc                       ; 0xc116f vgabios.c:731
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1171
    jmp short 011a8h                          ; eb 33                       ; 0xc1173 vgabios.c:733
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc1175 vgabios.c:734
    xor al, al                                ; 30 c0                       ; 0xc1178
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc117a
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc117d
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1180
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1183
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc1186
    cmp si, cx                                ; 39 ce                       ; 0xc1189
    jnc short 011aah                          ; 73 1d                       ; 0xc118b
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc118d
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1190
    mov si, cx                                ; 89 ce                       ; 0xc1193
    dec si                                    ; 4e                          ; 0xc1195
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc1196
    je short 011e4h                           ; 74 49                       ; 0xc1199
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc119b
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc119e
    dec cx                                    ; 49                          ; 0xc11a1
    dec cx                                    ; 49                          ; 0xc11a2
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc11a3
    jne short 011aah                          ; 75 02                       ; 0xc11a6
    jmp short 011e4h                          ; eb 3a                       ; 0xc11a8
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc11aa vgabios.c:736
    jbe short 011e4h                          ; 76 35                       ; 0xc11ad
    mov cl, bl                                ; 88 d9                       ; 0xc11af vgabios.c:737
    xor ch, ch                                ; 30 ed                       ; 0xc11b1
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc11b3
    mov byte [bp-009h], ch                    ; 88 6e f7                    ; 0xc11b6
    mov si, cx                                ; 89 ce                       ; 0xc11b9
    inc si                                    ; 46                          ; 0xc11bb
    inc si                                    ; 46                          ; 0xc11bc
    mov cl, dl                                ; 88 d1                       ; 0xc11bd
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc11bf
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc11c1
    jl short 011d9h                           ; 7c 13                       ; 0xc11c4
    sub bl, ah                                ; 28 e3                       ; 0xc11c6 vgabios.c:739
    add bl, dl                                ; 00 d3                       ; 0xc11c8
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11ca
    mov ah, cl                                ; 88 cc                       ; 0xc11cc vgabios.c:740
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc11ce vgabios.c:741
    jc short 011e4h                           ; 72 11                       ; 0xc11d1
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc11d3 vgabios.c:743
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11d5 vgabios.c:744
    jmp short 011e4h                          ; eb 0b                       ; 0xc11d7 vgabios.c:746
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc11d9
    jbe short 011e2h                          ; 76 04                       ; 0xc11dc
    shr dx, 1                                 ; d1 ea                       ; 0xc11de vgabios.c:748
    mov bl, dl                                ; 88 d3                       ; 0xc11e0
    mov ah, cl                                ; 88 cc                       ; 0xc11e2 vgabios.c:752
    mov si, strict word 00063h                ; be 63 00                    ; 0xc11e4 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc11e7
    mov es, dx                                ; 8e c2                       ; 0xc11ea
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11ec
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc11ef vgabios.c:763
    mov dx, cx                                ; 89 ca                       ; 0xc11f1
    out DX, AL                                ; ee                          ; 0xc11f3
    mov si, cx                                ; 89 ce                       ; 0xc11f4 vgabios.c:764
    inc si                                    ; 46                          ; 0xc11f6
    mov al, bl                                ; 88 d8                       ; 0xc11f7
    mov dx, si                                ; 89 f2                       ; 0xc11f9
    out DX, AL                                ; ee                          ; 0xc11fb
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc11fc vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc11fe
    out DX, AL                                ; ee                          ; 0xc1200
    mov al, ah                                ; 88 e0                       ; 0xc1201 vgabios.c:766
    mov dx, si                                ; 89 f2                       ; 0xc1203
    out DX, AL                                ; ee                          ; 0xc1205
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc1206 vgabios.c:767
    pop si                                    ; 5e                          ; 0xc1209
    pop cx                                    ; 59                          ; 0xc120a
    pop bx                                    ; 5b                          ; 0xc120b
    pop bp                                    ; 5d                          ; 0xc120c
    retn                                      ; c3                          ; 0xc120d
  ; disGetNextSymbol 0xc120e LB 0x3075 -> off=0x0 cb=000000000000008f uValue=00000000000c120e 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc120e LB 0x8f
    push bp                                   ; 55                          ; 0xc120e vgabios.c:770
    mov bp, sp                                ; 89 e5                       ; 0xc120f
    push bx                                   ; 53                          ; 0xc1211
    push cx                                   ; 51                          ; 0xc1212
    push si                                   ; 56                          ; 0xc1213
    push di                                   ; 57                          ; 0xc1214
    push ax                                   ; 50                          ; 0xc1215
    mov bl, al                                ; 88 c3                       ; 0xc1216
    mov cx, dx                                ; 89 d1                       ; 0xc1218
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc121a vgabios.c:776
    jnbe short 01294h                         ; 77 76                       ; 0xc121c
    xor ah, ah                                ; 30 e4                       ; 0xc121e vgabios.c:779
    mov si, ax                                ; 89 c6                       ; 0xc1220
    add si, ax                                ; 01 c6                       ; 0xc1222
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc1224
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1227 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc122a
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc122c
    mov si, strict word 00062h                ; be 62 00                    ; 0xc122f vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1232
    cmp bl, al                                ; 38 c3                       ; 0xc1235 vgabios.c:783
    jne short 01294h                          ; 75 5b                       ; 0xc1237
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc1239 vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc123c
    mov si, 00084h                            ; be 84 00                    ; 0xc123f vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1242
    xor ah, ah                                ; 30 e4                       ; 0xc1245 vgabios.c:38
    mov si, ax                                ; 89 c6                       ; 0xc1247
    inc si                                    ; 46                          ; 0xc1249
    mov ax, dx                                ; 89 d0                       ; 0xc124a vgabios.c:789
    xor al, dl                                ; 30 d0                       ; 0xc124c
    shr ax, 008h                              ; c1 e8 08                    ; 0xc124e
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1251
    mov ax, di                                ; 89 f8                       ; 0xc1254 vgabios.c:792
    mul si                                    ; f7 e6                       ; 0xc1256
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1258
    xor bh, bh                                ; 30 ff                       ; 0xc125a
    inc ax                                    ; 40                          ; 0xc125c
    mul bx                                    ; f7 e3                       ; 0xc125d
    mov bl, cl                                ; 88 cb                       ; 0xc125f
    mov si, bx                                ; 89 de                       ; 0xc1261
    add si, ax                                ; 01 c6                       ; 0xc1263
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1265
    xor ah, ah                                ; 30 e4                       ; 0xc1268
    mul di                                    ; f7 e7                       ; 0xc126a
    add si, ax                                ; 01 c6                       ; 0xc126c
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc126e vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1271
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc1274 vgabios.c:796
    mov dx, bx                                ; 89 da                       ; 0xc1276
    out DX, AL                                ; ee                          ; 0xc1278
    mov ax, si                                ; 89 f0                       ; 0xc1279 vgabios.c:797
    xor al, al                                ; 30 c0                       ; 0xc127b
    shr ax, 008h                              ; c1 e8 08                    ; 0xc127d
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc1280
    mov dx, cx                                ; 89 ca                       ; 0xc1283
    out DX, AL                                ; ee                          ; 0xc1285
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1286 vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc1288
    out DX, AL                                ; ee                          ; 0xc128a
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc128b vgabios.c:799
    mov ax, si                                ; 89 f0                       ; 0xc128f
    mov dx, cx                                ; 89 ca                       ; 0xc1291
    out DX, AL                                ; ee                          ; 0xc1293
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1294 vgabios.c:801
    pop di                                    ; 5f                          ; 0xc1297
    pop si                                    ; 5e                          ; 0xc1298
    pop cx                                    ; 59                          ; 0xc1299
    pop bx                                    ; 5b                          ; 0xc129a
    pop bp                                    ; 5d                          ; 0xc129b
    retn                                      ; c3                          ; 0xc129c
  ; disGetNextSymbol 0xc129d LB 0x2fe6 -> off=0x0 cb=00000000000000d8 uValue=00000000000c129d 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc129d LB 0xd8
    push bp                                   ; 55                          ; 0xc129d vgabios.c:804
    mov bp, sp                                ; 89 e5                       ; 0xc129e
    push bx                                   ; 53                          ; 0xc12a0
    push cx                                   ; 51                          ; 0xc12a1
    push dx                                   ; 52                          ; 0xc12a2
    push si                                   ; 56                          ; 0xc12a3
    push di                                   ; 57                          ; 0xc12a4
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc12a5
    mov cl, al                                ; 88 c1                       ; 0xc12a8
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12aa vgabios.c:810
    jnbe short 012c4h                         ; 77 16                       ; 0xc12ac
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc12ae vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12b1
    mov es, ax                                ; 8e c0                       ; 0xc12b4
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12b6
    xor ah, ah                                ; 30 e4                       ; 0xc12b9 vgabios.c:814
    call 035d1h                               ; e8 13 23                    ; 0xc12bb
    mov ch, al                                ; 88 c5                       ; 0xc12be
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc12c0 vgabios.c:815
    jne short 012c7h                          ; 75 03                       ; 0xc12c2
    jmp near 0136bh                           ; e9 a4 00                    ; 0xc12c4
    mov al, cl                                ; 88 c8                       ; 0xc12c7 vgabios.c:818
    xor ah, ah                                ; 30 e4                       ; 0xc12c9
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc12cb
    lea dx, [bp-010h]                         ; 8d 56 f0                    ; 0xc12ce
    call 00a0bh                               ; e8 37 f7                    ; 0xc12d1
    mov bl, ch                                ; 88 eb                       ; 0xc12d4 vgabios.c:820
    xor bh, bh                                ; 30 ff                       ; 0xc12d6
    mov si, bx                                ; 89 de                       ; 0xc12d8
    sal si, 003h                              ; c1 e6 03                    ; 0xc12da
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc12dd
    jne short 01323h                          ; 75 3f                       ; 0xc12e2
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc12e4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12e7
    mov es, ax                                ; 8e c0                       ; 0xc12ea
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc12ec
    mov bx, 00084h                            ; bb 84 00                    ; 0xc12ef vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12f2
    xor ah, ah                                ; 30 e4                       ; 0xc12f5 vgabios.c:38
    mov bx, ax                                ; 89 c3                       ; 0xc12f7
    inc bx                                    ; 43                          ; 0xc12f9
    mov ax, dx                                ; 89 d0                       ; 0xc12fa vgabios.c:827
    mul bx                                    ; f7 e3                       ; 0xc12fc
    mov di, ax                                ; 89 c7                       ; 0xc12fe
    add ax, ax                                ; 01 c0                       ; 0xc1300
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1302
    mov byte [bp-00ch], cl                    ; 88 4e f4                    ; 0xc1304
    mov byte [bp-00bh], 000h                  ; c6 46 f5 00                 ; 0xc1307
    inc ax                                    ; 40                          ; 0xc130b
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc130c
    mov bx, ax                                ; 89 c3                       ; 0xc130f
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc1311 vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc1314
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc1317 vgabios.c:831
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc131b
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc131e
    jmp short 01332h                          ; eb 0f                       ; 0xc1321 vgabios.c:833
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc1323 vgabios.c:835
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1327
    mov al, cl                                ; 88 c8                       ; 0xc132a
    xor ah, ah                                ; 30 e4                       ; 0xc132c
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc132e
    mov bx, ax                                ; 89 c3                       ; 0xc1332
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1334 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1337
    mov es, ax                                ; 8e c0                       ; 0xc133a
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc133c
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc133f vgabios.c:840
    mov dx, si                                ; 89 f2                       ; 0xc1341
    out DX, AL                                ; ee                          ; 0xc1343
    mov ax, bx                                ; 89 d8                       ; 0xc1344 vgabios.c:841
    xor al, bl                                ; 30 d8                       ; 0xc1346
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1348
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc134b
    mov dx, di                                ; 89 fa                       ; 0xc134e
    out DX, AL                                ; ee                          ; 0xc1350
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc1351 vgabios.c:842
    mov dx, si                                ; 89 f2                       ; 0xc1353
    out DX, AL                                ; ee                          ; 0xc1355
    xor bh, bh                                ; 30 ff                       ; 0xc1356 vgabios.c:843
    mov ax, bx                                ; 89 d8                       ; 0xc1358
    mov dx, di                                ; 89 fa                       ; 0xc135a
    out DX, AL                                ; ee                          ; 0xc135c
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc135d vgabios.c:42
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc1360
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1363 vgabios.c:853
    mov al, cl                                ; 88 c8                       ; 0xc1366
    call 0120eh                               ; e8 a3 fe                    ; 0xc1368
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc136b vgabios.c:854
    pop di                                    ; 5f                          ; 0xc136e
    pop si                                    ; 5e                          ; 0xc136f
    pop dx                                    ; 5a                          ; 0xc1370
    pop cx                                    ; 59                          ; 0xc1371
    pop bx                                    ; 5b                          ; 0xc1372
    pop bp                                    ; 5d                          ; 0xc1373
    retn                                      ; c3                          ; 0xc1374
  ; disGetNextSymbol 0xc1375 LB 0x2f0e -> off=0x0 cb=0000000000000384 uValue=00000000000c1375 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc1375 LB 0x384
    push bp                                   ; 55                          ; 0xc1375 vgabios.c:874
    mov bp, sp                                ; 89 e5                       ; 0xc1376
    push bx                                   ; 53                          ; 0xc1378
    push cx                                   ; 51                          ; 0xc1379
    push dx                                   ; 52                          ; 0xc137a
    push si                                   ; 56                          ; 0xc137b
    push di                                   ; 57                          ; 0xc137c
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc137d
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1380
    and AL, strict byte 080h                  ; 24 80                       ; 0xc1383 vgabios.c:878
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1385
    call 007bfh                               ; e8 34 f4                    ; 0xc1388 vgabios.c:885
    test ax, ax                               ; 85 c0                       ; 0xc138b
    je short 0139bh                           ; 74 0c                       ; 0xc138d
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc138f vgabios.c:887
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1391
    out DX, AL                                ; ee                          ; 0xc1394
    xor al, al                                ; 30 c0                       ; 0xc1395 vgabios.c:888
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1397
    out DX, AL                                ; ee                          ; 0xc139a
    and byte [bp-00eh], 07fh                  ; 80 66 f2 7f                 ; 0xc139b vgabios.c:893
    cmp byte [bp-00eh], 007h                  ; 80 7e f2 07                 ; 0xc139f vgabios.c:897
    jne short 013a9h                          ; 75 04                       ; 0xc13a3
    mov byte [bp-00eh], 000h                  ; c6 46 f2 00                 ; 0xc13a5 vgabios.c:898
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc13a9 vgabios.c:901
    xor ah, ah                                ; 30 e4                       ; 0xc13ac
    call 035d1h                               ; e8 20 22                    ; 0xc13ae
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc13b1
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc13b4 vgabios.c:907
    je short 0141dh                           ; 74 65                       ; 0xc13b6
    mov dl, al                                ; 88 c2                       ; 0xc13b8 vgabios.c:910
    xor dh, dh                                ; 30 f6                       ; 0xc13ba
    mov bx, dx                                ; 89 d3                       ; 0xc13bc
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc13be
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc13c2
    mov bl, al                                ; 88 c3                       ; 0xc13c5 vgabios.c:911
    sal bx, 006h                              ; c1 e3 06                    ; 0xc13c7
    mov al, byte [bx+04842h]                  ; 8a 87 42 48                 ; 0xc13ca
    xor ah, ah                                ; 30 e4                       ; 0xc13ce
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc13d0
    mov al, byte [bx+04843h]                  ; 8a 87 43 48                 ; 0xc13d3 vgabios.c:912
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc13d7
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc13da vgabios.c:913
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc13de
    mov bx, 00089h                            ; bb 89 00                    ; 0xc13e1 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13e4
    mov es, ax                                ; 8e c0                       ; 0xc13e7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc13e9
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc13ec vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc13ef vgabios.c:928
    jne short 0143ah                          ; 75 47                       ; 0xc13f1
    mov bx, dx                                ; 89 d3                       ; 0xc13f3 vgabios.c:930
    sal bx, 003h                              ; c1 e3 03                    ; 0xc13f5
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc13f8
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc13fc
    out DX, AL                                ; ee                          ; 0xc13ff
    xor al, al                                ; 30 c0                       ; 0xc1400 vgabios.c:933
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1402
    out DX, AL                                ; ee                          ; 0xc1405
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc1406 vgabios.c:936
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc140a
    jc short 01420h                           ; 72 11                       ; 0xc140d
    jbe short 01429h                          ; 76 18                       ; 0xc140f
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1411
    je short 01433h                           ; 74 1d                       ; 0xc1414
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1416
    je short 0142eh                           ; 74 13                       ; 0xc1419
    jmp short 01436h                          ; eb 19                       ; 0xc141b
    jmp near 016efh                           ; e9 cf 02                    ; 0xc141d
    test bl, bl                               ; 84 db                       ; 0xc1420
    jne short 01436h                          ; 75 12                       ; 0xc1422
    mov si, 04fc2h                            ; be c2 4f                    ; 0xc1424 vgabios.c:938
    jmp short 01436h                          ; eb 0d                       ; 0xc1427 vgabios.c:939
    mov si, 05082h                            ; be 82 50                    ; 0xc1429 vgabios.c:941
    jmp short 01436h                          ; eb 08                       ; 0xc142c vgabios.c:942
    mov si, 05142h                            ; be 42 51                    ; 0xc142e vgabios.c:944
    jmp short 01436h                          ; eb 03                       ; 0xc1431 vgabios.c:945
    mov si, 05202h                            ; be 02 52                    ; 0xc1433 vgabios.c:947
    xor cx, cx                                ; 31 c9                       ; 0xc1436 vgabios.c:951
    jmp short 01442h                          ; eb 08                       ; 0xc1438
    jmp short 01488h                          ; eb 4c                       ; 0xc143a
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc143c
    jnc short 0147ah                          ; 73 38                       ; 0xc1440
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1442 vgabios.c:952
    xor bh, bh                                ; 30 ff                       ; 0xc1445
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1447
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc144a
    xor bh, bh                                ; 30 ff                       ; 0xc144e
    mov al, byte [bx+0483eh]                  ; 8a 87 3e 48                 ; 0xc1450
    xor ah, ah                                ; 30 e4                       ; 0xc1454
    cmp cx, ax                                ; 39 c1                       ; 0xc1456
    jnbe short 0146fh                         ; 77 15                       ; 0xc1458
    imul bx, cx, strict byte 00003h           ; 6b d9 03                    ; 0xc145a vgabios.c:953
    add bx, si                                ; 01 f3                       ; 0xc145d
    mov al, byte [bx]                         ; 8a 07                       ; 0xc145f
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1461
    out DX, AL                                ; ee                          ; 0xc1464
    mov al, byte [bx+001h]                    ; 8a 47 01                    ; 0xc1465 vgabios.c:954
    out DX, AL                                ; ee                          ; 0xc1468
    mov al, byte [bx+002h]                    ; 8a 47 02                    ; 0xc1469 vgabios.c:955
    out DX, AL                                ; ee                          ; 0xc146c
    jmp short 01477h                          ; eb 08                       ; 0xc146d vgabios.c:957
    xor al, al                                ; 30 c0                       ; 0xc146f vgabios.c:958
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1471
    out DX, AL                                ; ee                          ; 0xc1474
    out DX, AL                                ; ee                          ; 0xc1475 vgabios.c:959
    out DX, AL                                ; ee                          ; 0xc1476 vgabios.c:960
    inc cx                                    ; 41                          ; 0xc1477 vgabios.c:962
    jmp short 0143ch                          ; eb c2                       ; 0xc1478
    test byte [bp-014h], 002h                 ; f6 46 ec 02                 ; 0xc147a vgabios.c:963
    je short 01488h                           ; 74 08                       ; 0xc147e
    mov dx, 00100h                            ; ba 00 01                    ; 0xc1480 vgabios.c:965
    xor ax, ax                                ; 31 c0                       ; 0xc1483
    call 0107ah                               ; e8 f2 fb                    ; 0xc1485
    mov dx, 003dah                            ; ba da 03                    ; 0xc1488 vgabios.c:970
    in AL, DX                                 ; ec                          ; 0xc148b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc148c
    xor cx, cx                                ; 31 c9                       ; 0xc148e vgabios.c:973
    jmp short 01497h                          ; eb 05                       ; 0xc1490
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc1492
    jnbe short 014b1h                         ; 77 1a                       ; 0xc1495
    mov al, cl                                ; 88 c8                       ; 0xc1497 vgabios.c:974
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1499
    out DX, AL                                ; ee                          ; 0xc149c
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc149d vgabios.c:975
    xor ah, ah                                ; 30 e4                       ; 0xc14a0
    mov bx, ax                                ; 89 c3                       ; 0xc14a2
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14a4
    add bx, cx                                ; 01 cb                       ; 0xc14a7
    mov al, byte [bx+04865h]                  ; 8a 87 65 48                 ; 0xc14a9
    out DX, AL                                ; ee                          ; 0xc14ad
    inc cx                                    ; 41                          ; 0xc14ae vgabios.c:976
    jmp short 01492h                          ; eb e1                       ; 0xc14af
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc14b1 vgabios.c:977
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14b3
    out DX, AL                                ; ee                          ; 0xc14b6
    xor al, al                                ; 30 c0                       ; 0xc14b7 vgabios.c:978
    out DX, AL                                ; ee                          ; 0xc14b9
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14ba vgabios.c:981
    out DX, AL                                ; ee                          ; 0xc14bd
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc14be vgabios.c:982
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc14c0
    out DX, AL                                ; ee                          ; 0xc14c3
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc14c4 vgabios.c:983
    jmp short 014ceh                          ; eb 05                       ; 0xc14c7
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc14c9
    jnbe short 014ebh                         ; 77 1d                       ; 0xc14cc
    mov al, cl                                ; 88 c8                       ; 0xc14ce vgabios.c:984
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14d0
    out DX, AL                                ; ee                          ; 0xc14d3
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc14d4 vgabios.c:985
    xor ah, ah                                ; 30 e4                       ; 0xc14d7
    mov bx, ax                                ; 89 c3                       ; 0xc14d9
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14db
    add bx, cx                                ; 01 cb                       ; 0xc14de
    mov al, byte [bx+04846h]                  ; 8a 87 46 48                 ; 0xc14e0
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc14e4
    out DX, AL                                ; ee                          ; 0xc14e7
    inc cx                                    ; 41                          ; 0xc14e8 vgabios.c:986
    jmp short 014c9h                          ; eb de                       ; 0xc14e9
    xor cx, cx                                ; 31 c9                       ; 0xc14eb vgabios.c:989
    jmp short 014f4h                          ; eb 05                       ; 0xc14ed
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc14ef
    jnbe short 01511h                         ; 77 1d                       ; 0xc14f2
    mov al, cl                                ; 88 c8                       ; 0xc14f4 vgabios.c:990
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc14f6
    out DX, AL                                ; ee                          ; 0xc14f9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc14fa vgabios.c:991
    xor ah, ah                                ; 30 e4                       ; 0xc14fd
    mov bx, ax                                ; 89 c3                       ; 0xc14ff
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1501
    add bx, cx                                ; 01 cb                       ; 0xc1504
    mov al, byte [bx+04879h]                  ; 8a 87 79 48                 ; 0xc1506
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc150a
    out DX, AL                                ; ee                          ; 0xc150d
    inc cx                                    ; 41                          ; 0xc150e vgabios.c:992
    jmp short 014efh                          ; eb de                       ; 0xc150f
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1511 vgabios.c:995
    xor bh, bh                                ; 30 ff                       ; 0xc1514
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1516
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc1519
    jne short 01525h                          ; 75 05                       ; 0xc151e
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1520
    jmp short 01528h                          ; eb 03                       ; 0xc1523
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc1525
    mov si, dx                                ; 89 d6                       ; 0xc1528
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc152a vgabios.c:998
    out DX, ax                                ; ef                          ; 0xc152d
    xor cx, cx                                ; 31 c9                       ; 0xc152e vgabios.c:1000
    jmp short 01537h                          ; eb 05                       ; 0xc1530
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc1532
    jnbe short 01553h                         ; 77 1c                       ; 0xc1535
    mov al, cl                                ; 88 c8                       ; 0xc1537 vgabios.c:1001
    mov dx, si                                ; 89 f2                       ; 0xc1539
    out DX, AL                                ; ee                          ; 0xc153b
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc153c vgabios.c:1002
    xor bh, bh                                ; 30 ff                       ; 0xc153f
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1541
    mov di, bx                                ; 89 df                       ; 0xc1544
    add di, cx                                ; 01 cf                       ; 0xc1546
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc1548
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc154b
    out DX, AL                                ; ee                          ; 0xc154f
    inc cx                                    ; 41                          ; 0xc1550 vgabios.c:1003
    jmp short 01532h                          ; eb df                       ; 0xc1551
    mov al, byte [bx+0484bh]                  ; 8a 87 4b 48                 ; 0xc1553 vgabios.c:1006
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1557
    out DX, AL                                ; ee                          ; 0xc155a
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc155b vgabios.c:1009
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc155d
    out DX, AL                                ; ee                          ; 0xc1560
    mov dx, 003dah                            ; ba da 03                    ; 0xc1561 vgabios.c:1010
    in AL, DX                                 ; ec                          ; 0xc1564
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1565
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc1567 vgabios.c:1012
    jne short 015cdh                          ; 75 60                       ; 0xc156b
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc156d vgabios.c:1014
    xor bh, bh                                ; 30 ff                       ; 0xc1570
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1572
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1575
    jne short 0158fh                          ; 75 13                       ; 0xc157a
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc157c vgabios.c:1016
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1580
    mov ax, 00720h                            ; b8 20 07                    ; 0xc1583
    xor di, di                                ; 31 ff                       ; 0xc1586
    cld                                       ; fc                          ; 0xc1588
    jcxz 0158dh                               ; e3 02                       ; 0xc1589
    rep stosw                                 ; f3 ab                       ; 0xc158b
    jmp short 015cdh                          ; eb 3e                       ; 0xc158d vgabios.c:1018
    cmp byte [bp-00eh], 00dh                  ; 80 7e f2 0d                 ; 0xc158f vgabios.c:1020
    jnc short 015a7h                          ; 73 12                       ; 0xc1593
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1595 vgabios.c:1022
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1599
    xor ax, ax                                ; 31 c0                       ; 0xc159c
    xor di, di                                ; 31 ff                       ; 0xc159e
    cld                                       ; fc                          ; 0xc15a0
    jcxz 015a5h                               ; e3 02                       ; 0xc15a1
    rep stosw                                 ; f3 ab                       ; 0xc15a3
    jmp short 015cdh                          ; eb 26                       ; 0xc15a5 vgabios.c:1024
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc15a7 vgabios.c:1026
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc15a9
    out DX, AL                                ; ee                          ; 0xc15ac
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc15ad vgabios.c:1027
    in AL, DX                                 ; ec                          ; 0xc15b0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc15b1
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc15b3
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc15b6 vgabios.c:1028
    out DX, AL                                ; ee                          ; 0xc15b8
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15b9 vgabios.c:1029
    mov cx, 08000h                            ; b9 00 80                    ; 0xc15bd
    xor ax, ax                                ; 31 c0                       ; 0xc15c0
    xor di, di                                ; 31 ff                       ; 0xc15c2
    cld                                       ; fc                          ; 0xc15c4
    jcxz 015c9h                               ; e3 02                       ; 0xc15c5
    rep stosw                                 ; f3 ab                       ; 0xc15c7
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc15c9 vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc15cc
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc15cd vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15d0
    mov es, ax                                ; 8e c0                       ; 0xc15d3
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc15d5
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15d8
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc15db vgabios.c:52
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc15de
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc15e1
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc15e4 vgabios.c:1038
    xor bh, bh                                ; 30 ff                       ; 0xc15e7
    sal bx, 006h                              ; c1 e3 06                    ; 0xc15e9
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc15ec vgabios.c:50
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc15f0 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc15f3
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc15f6 vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc15f9
    mov bx, 00084h                            ; bb 84 00                    ; 0xc15fc vgabios.c:42
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc15ff
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1602
    mov bx, 00085h                            ; bb 85 00                    ; 0xc1605 vgabios.c:52
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1608
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc160b
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc160e vgabios.c:1042
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1611
    mov bx, 00087h                            ; bb 87 00                    ; 0xc1613 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1616
    mov bx, 00088h                            ; bb 88 00                    ; 0xc1619 vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc161c
    mov bx, 00089h                            ; bb 89 00                    ; 0xc1620 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1623
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc1626 vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1628 vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc162b vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc162e
    mov ax, ds                                ; 8c d8                       ; 0xc1632 vgabios.c:1048
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc1634 vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc1637
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc163c
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1640 vgabios.c:1050
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1643
    jnbe short 0166eh                         ; 77 27                       ; 0xc1645
    mov bl, al                                ; 88 c3                       ; 0xc1647 vgabios.c:1052
    xor bh, bh                                ; 30 ff                       ; 0xc1649
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc164b vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc164f vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1652
    cmp byte [bp-00eh], 006h                  ; 80 7e f2 06                 ; 0xc1655 vgabios.c:1053
    jne short 01660h                          ; 75 05                       ; 0xc1659
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc165b
    jmp short 01663h                          ; eb 03                       ; 0xc165e
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc1660
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc1663 vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1666
    mov es, dx                                ; 8e c2                       ; 0xc1669
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc166b
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc166e vgabios.c:1057
    xor bh, bh                                ; 30 ff                       ; 0xc1671
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1673
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1676
    jne short 01686h                          ; 75 09                       ; 0xc167b
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc167d vgabios.c:1059
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc1680
    call 01107h                               ; e8 81 fa                    ; 0xc1683
    xor cx, cx                                ; 31 c9                       ; 0xc1686 vgabios.c:1063
    jmp short 0168fh                          ; eb 05                       ; 0xc1688
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc168a
    jnc short 0169bh                          ; 73 0c                       ; 0xc168d
    mov al, cl                                ; 88 c8                       ; 0xc168f vgabios.c:1064
    xor ah, ah                                ; 30 e4                       ; 0xc1691
    xor dx, dx                                ; 31 d2                       ; 0xc1693
    call 0120eh                               ; e8 76 fb                    ; 0xc1695
    inc cx                                    ; 41                          ; 0xc1698
    jmp short 0168ah                          ; eb ef                       ; 0xc1699
    xor ax, ax                                ; 31 c0                       ; 0xc169b vgabios.c:1067
    call 0129dh                               ; e8 fd fb                    ; 0xc169d
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16a0 vgabios.c:1070
    xor bh, bh                                ; 30 ff                       ; 0xc16a3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc16a5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16a8
    jne short 016bfh                          ; 75 10                       ; 0xc16ad
    xor bl, bl                                ; 30 db                       ; 0xc16af vgabios.c:1072
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc16b1
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc16b3
    int 010h                                  ; cd 10                       ; 0xc16b5
    xor bl, bl                                ; 30 db                       ; 0xc16b7 vgabios.c:1073
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc16b9
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc16bb
    int 010h                                  ; cd 10                       ; 0xc16bd
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc16bf vgabios.c:1077
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc16c2
    call 00980h                               ; e8 b8 f2                    ; 0xc16c5
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc16c8 vgabios.c:1079
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc16cb
    je short 016eah                           ; 74 1a                       ; 0xc16ce
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc16d0
    je short 016e5h                           ; 74 10                       ; 0xc16d3
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc16d5
    jne short 016efh                          ; 75 15                       ; 0xc16d8
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc16da vgabios.c:1081
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc16dd
    call 00980h                               ; e8 9d f2                    ; 0xc16e0
    jmp short 016efh                          ; eb 0a                       ; 0xc16e3 vgabios.c:1082
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc16e5 vgabios.c:1084
    jmp short 016ddh                          ; eb f3                       ; 0xc16e8
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc16ea vgabios.c:1087
    jmp short 016ddh                          ; eb ee                       ; 0xc16ed
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc16ef vgabios.c:1090
    pop di                                    ; 5f                          ; 0xc16f2
    pop si                                    ; 5e                          ; 0xc16f3
    pop dx                                    ; 5a                          ; 0xc16f4
    pop cx                                    ; 59                          ; 0xc16f5
    pop bx                                    ; 5b                          ; 0xc16f6
    pop bp                                    ; 5d                          ; 0xc16f7
    retn                                      ; c3                          ; 0xc16f8
  ; disGetNextSymbol 0xc16f9 LB 0x2b8a -> off=0x0 cb=000000000000008f uValue=00000000000c16f9 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc16f9 LB 0x8f
    push bp                                   ; 55                          ; 0xc16f9 vgabios.c:1093
    mov bp, sp                                ; 89 e5                       ; 0xc16fa
    push si                                   ; 56                          ; 0xc16fc
    push di                                   ; 57                          ; 0xc16fd
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc16fe
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1701
    mov al, dl                                ; 88 d0                       ; 0xc1704
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1706
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1709
    xor ah, ah                                ; 30 e4                       ; 0xc170c vgabios.c:1099
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc170e
    xor dh, dh                                ; 30 f6                       ; 0xc1711
    mov cx, dx                                ; 89 d1                       ; 0xc1713
    imul dx                                   ; f7 ea                       ; 0xc1715
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1717
    xor dh, dh                                ; 30 f6                       ; 0xc171a
    mov si, dx                                ; 89 d6                       ; 0xc171c
    imul dx                                   ; f7 ea                       ; 0xc171e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1720
    xor dh, dh                                ; 30 f6                       ; 0xc1723
    mov bx, dx                                ; 89 d3                       ; 0xc1725
    add ax, dx                                ; 01 d0                       ; 0xc1727
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1729
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc172c vgabios.c:1100
    xor ah, ah                                ; 30 e4                       ; 0xc172f
    imul cx                                   ; f7 e9                       ; 0xc1731
    imul si                                   ; f7 ee                       ; 0xc1733
    add ax, bx                                ; 01 d8                       ; 0xc1735
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1737
    mov ax, 00105h                            ; b8 05 01                    ; 0xc173a vgabios.c:1101
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc173d
    out DX, ax                                ; ef                          ; 0xc1740
    xor bl, bl                                ; 30 db                       ; 0xc1741 vgabios.c:1102
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1743
    jnc short 01778h                          ; 73 30                       ; 0xc1746
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1748 vgabios.c:1104
    xor ah, ah                                ; 30 e4                       ; 0xc174b
    mov cx, ax                                ; 89 c1                       ; 0xc174d
    mov al, bl                                ; 88 d8                       ; 0xc174f
    mov dx, ax                                ; 89 c2                       ; 0xc1751
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1753
    mov si, ax                                ; 89 c6                       ; 0xc1756
    mov ax, dx                                ; 89 d0                       ; 0xc1758
    imul si                                   ; f7 ee                       ; 0xc175a
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc175c
    add si, ax                                ; 01 c6                       ; 0xc175f
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1761
    add di, ax                                ; 01 c7                       ; 0xc1764
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1766
    mov es, dx                                ; 8e c2                       ; 0xc1769
    cld                                       ; fc                          ; 0xc176b
    jcxz 01774h                               ; e3 06                       ; 0xc176c
    push DS                                   ; 1e                          ; 0xc176e
    mov ds, dx                                ; 8e da                       ; 0xc176f
    rep movsb                                 ; f3 a4                       ; 0xc1771
    pop DS                                    ; 1f                          ; 0xc1773
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1774 vgabios.c:1105
    jmp short 01743h                          ; eb cb                       ; 0xc1776
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1778 vgabios.c:1106
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc177b
    out DX, ax                                ; ef                          ; 0xc177e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc177f vgabios.c:1107
    pop di                                    ; 5f                          ; 0xc1782
    pop si                                    ; 5e                          ; 0xc1783
    pop bp                                    ; 5d                          ; 0xc1784
    retn 00004h                               ; c2 04 00                    ; 0xc1785
  ; disGetNextSymbol 0xc1788 LB 0x2afb -> off=0x0 cb=000000000000007c uValue=00000000000c1788 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc1788 LB 0x7c
    push bp                                   ; 55                          ; 0xc1788 vgabios.c:1110
    mov bp, sp                                ; 89 e5                       ; 0xc1789
    push si                                   ; 56                          ; 0xc178b
    push di                                   ; 57                          ; 0xc178c
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc178d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1790
    mov al, dl                                ; 88 d0                       ; 0xc1793
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1795
    mov bh, cl                                ; 88 cf                       ; 0xc1798
    xor ah, ah                                ; 30 e4                       ; 0xc179a vgabios.c:1116
    mov dx, ax                                ; 89 c2                       ; 0xc179c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc179e
    mov cx, ax                                ; 89 c1                       ; 0xc17a1
    mov ax, dx                                ; 89 d0                       ; 0xc17a3
    imul cx                                   ; f7 e9                       ; 0xc17a5
    mov dl, bh                                ; 88 fa                       ; 0xc17a7
    xor dh, dh                                ; 30 f6                       ; 0xc17a9
    imul dx                                   ; f7 ea                       ; 0xc17ab
    mov dx, ax                                ; 89 c2                       ; 0xc17ad
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17af
    xor ah, ah                                ; 30 e4                       ; 0xc17b2
    add dx, ax                                ; 01 c2                       ; 0xc17b4
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc17b6
    mov ax, 00205h                            ; b8 05 02                    ; 0xc17b9 vgabios.c:1117
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17bc
    out DX, ax                                ; ef                          ; 0xc17bf
    xor bl, bl                                ; 30 db                       ; 0xc17c0 vgabios.c:1118
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc17c2
    jnc short 017f4h                          ; 73 2d                       ; 0xc17c5
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc17c7 vgabios.c:1120
    xor ch, ch                                ; 30 ed                       ; 0xc17ca
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc17cc
    xor ah, ah                                ; 30 e4                       ; 0xc17cf
    mov si, ax                                ; 89 c6                       ; 0xc17d1
    mov al, bl                                ; 88 d8                       ; 0xc17d3
    mov dx, ax                                ; 89 c2                       ; 0xc17d5
    mov al, bh                                ; 88 f8                       ; 0xc17d7
    mov di, ax                                ; 89 c7                       ; 0xc17d9
    mov ax, dx                                ; 89 d0                       ; 0xc17db
    imul di                                   ; f7 ef                       ; 0xc17dd
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17df
    add di, ax                                ; 01 c7                       ; 0xc17e2
    mov ax, si                                ; 89 f0                       ; 0xc17e4
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc17e6
    mov es, dx                                ; 8e c2                       ; 0xc17e9
    cld                                       ; fc                          ; 0xc17eb
    jcxz 017f0h                               ; e3 02                       ; 0xc17ec
    rep stosb                                 ; f3 aa                       ; 0xc17ee
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc17f0 vgabios.c:1121
    jmp short 017c2h                          ; eb ce                       ; 0xc17f2
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc17f4 vgabios.c:1122
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17f7
    out DX, ax                                ; ef                          ; 0xc17fa
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17fb vgabios.c:1123
    pop di                                    ; 5f                          ; 0xc17fe
    pop si                                    ; 5e                          ; 0xc17ff
    pop bp                                    ; 5d                          ; 0xc1800
    retn 00004h                               ; c2 04 00                    ; 0xc1801
  ; disGetNextSymbol 0xc1804 LB 0x2a7f -> off=0x0 cb=00000000000000b8 uValue=00000000000c1804 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1804 LB 0xb8
    push bp                                   ; 55                          ; 0xc1804 vgabios.c:1126
    mov bp, sp                                ; 89 e5                       ; 0xc1805
    push si                                   ; 56                          ; 0xc1807
    push di                                   ; 57                          ; 0xc1808
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1809
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc180c
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc180f
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc1812
    mov al, dl                                ; 88 d0                       ; 0xc1815 vgabios.c:1132
    xor ah, ah                                ; 30 e4                       ; 0xc1817
    mov bx, ax                                ; 89 c3                       ; 0xc1819
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc181b
    mov si, ax                                ; 89 c6                       ; 0xc181e
    mov ax, bx                                ; 89 d8                       ; 0xc1820
    imul si                                   ; f7 ee                       ; 0xc1822
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1824
    mov di, bx                                ; 89 df                       ; 0xc1827
    imul bx                                   ; f7 eb                       ; 0xc1829
    mov dx, ax                                ; 89 c2                       ; 0xc182b
    sar dx, 1                                 ; d1 fa                       ; 0xc182d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc182f
    xor ah, ah                                ; 30 e4                       ; 0xc1832
    mov bx, ax                                ; 89 c3                       ; 0xc1834
    add dx, ax                                ; 01 c2                       ; 0xc1836
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1838
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc183b vgabios.c:1133
    imul si                                   ; f7 ee                       ; 0xc183e
    imul di                                   ; f7 ef                       ; 0xc1840
    sar ax, 1                                 ; d1 f8                       ; 0xc1842
    add ax, bx                                ; 01 d8                       ; 0xc1844
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1846
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc1849 vgabios.c:1134
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc184c
    xor ah, ah                                ; 30 e4                       ; 0xc184f
    cwd                                       ; 99                          ; 0xc1851
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1852
    sar ax, 1                                 ; d1 f8                       ; 0xc1854
    mov bx, ax                                ; 89 c3                       ; 0xc1856
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1858
    xor ah, ah                                ; 30 e4                       ; 0xc185b
    cmp ax, bx                                ; 39 d8                       ; 0xc185d
    jnl short 018b3h                          ; 7d 52                       ; 0xc185f
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1861 vgabios.c:1136
    xor bh, bh                                ; 30 ff                       ; 0xc1864
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc1866
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1869
    imul bx                                   ; f7 eb                       ; 0xc186c
    mov bx, ax                                ; 89 c3                       ; 0xc186e
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1870
    add si, ax                                ; 01 c6                       ; 0xc1873
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1875
    add di, ax                                ; 01 c7                       ; 0xc1878
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc187a
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc187d
    mov es, dx                                ; 8e c2                       ; 0xc1880
    cld                                       ; fc                          ; 0xc1882
    jcxz 0188bh                               ; e3 06                       ; 0xc1883
    push DS                                   ; 1e                          ; 0xc1885
    mov ds, dx                                ; 8e da                       ; 0xc1886
    rep movsb                                 ; f3 a4                       ; 0xc1888
    pop DS                                    ; 1f                          ; 0xc188a
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc188b vgabios.c:1137
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc188e
    add si, bx                                ; 01 de                       ; 0xc1892
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1894
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1897
    add di, bx                                ; 01 df                       ; 0xc189b
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc189d
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc18a0
    mov es, dx                                ; 8e c2                       ; 0xc18a3
    cld                                       ; fc                          ; 0xc18a5
    jcxz 018aeh                               ; e3 06                       ; 0xc18a6
    push DS                                   ; 1e                          ; 0xc18a8
    mov ds, dx                                ; 8e da                       ; 0xc18a9
    rep movsb                                 ; f3 a4                       ; 0xc18ab
    pop DS                                    ; 1f                          ; 0xc18ad
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18ae vgabios.c:1138
    jmp short 0184ch                          ; eb 99                       ; 0xc18b1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18b3 vgabios.c:1139
    pop di                                    ; 5f                          ; 0xc18b6
    pop si                                    ; 5e                          ; 0xc18b7
    pop bp                                    ; 5d                          ; 0xc18b8
    retn 00004h                               ; c2 04 00                    ; 0xc18b9
  ; disGetNextSymbol 0xc18bc LB 0x29c7 -> off=0x0 cb=0000000000000096 uValue=00000000000c18bc 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc18bc LB 0x96
    push bp                                   ; 55                          ; 0xc18bc vgabios.c:1142
    mov bp, sp                                ; 89 e5                       ; 0xc18bd
    push si                                   ; 56                          ; 0xc18bf
    push di                                   ; 57                          ; 0xc18c0
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc18c1
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc18c4
    mov al, dl                                ; 88 d0                       ; 0xc18c7
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc18c9
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc18cc
    xor ah, ah                                ; 30 e4                       ; 0xc18cf vgabios.c:1148
    mov dx, ax                                ; 89 c2                       ; 0xc18d1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc18d3
    mov bx, ax                                ; 89 c3                       ; 0xc18d6
    mov ax, dx                                ; 89 d0                       ; 0xc18d8
    imul bx                                   ; f7 eb                       ; 0xc18da
    mov dl, cl                                ; 88 ca                       ; 0xc18dc
    xor dh, dh                                ; 30 f6                       ; 0xc18de
    imul dx                                   ; f7 ea                       ; 0xc18e0
    mov dx, ax                                ; 89 c2                       ; 0xc18e2
    sar dx, 1                                 ; d1 fa                       ; 0xc18e4
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc18e6
    xor ah, ah                                ; 30 e4                       ; 0xc18e9
    add dx, ax                                ; 01 c2                       ; 0xc18eb
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc18ed
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc18f0 vgabios.c:1149
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc18f3
    xor ah, ah                                ; 30 e4                       ; 0xc18f6
    cwd                                       ; 99                          ; 0xc18f8
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc18f9
    sar ax, 1                                 ; d1 f8                       ; 0xc18fb
    mov dx, ax                                ; 89 c2                       ; 0xc18fd
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18ff
    xor ah, ah                                ; 30 e4                       ; 0xc1902
    cmp ax, dx                                ; 39 d0                       ; 0xc1904
    jnl short 01949h                          ; 7d 41                       ; 0xc1906
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1908 vgabios.c:1151
    xor bh, bh                                ; 30 ff                       ; 0xc190b
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc190d
    xor dh, dh                                ; 30 f6                       ; 0xc1910
    mov si, dx                                ; 89 d6                       ; 0xc1912
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1914
    imul dx                                   ; f7 ea                       ; 0xc1917
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1919
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc191c
    add di, ax                                ; 01 c7                       ; 0xc191f
    mov cx, bx                                ; 89 d9                       ; 0xc1921
    mov ax, si                                ; 89 f0                       ; 0xc1923
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1925
    mov es, dx                                ; 8e c2                       ; 0xc1928
    cld                                       ; fc                          ; 0xc192a
    jcxz 0192fh                               ; e3 02                       ; 0xc192b
    rep stosb                                 ; f3 aa                       ; 0xc192d
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc192f vgabios.c:1152
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1932
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc1936
    mov cx, bx                                ; 89 d9                       ; 0xc1939
    mov ax, si                                ; 89 f0                       ; 0xc193b
    mov es, dx                                ; 8e c2                       ; 0xc193d
    cld                                       ; fc                          ; 0xc193f
    jcxz 01944h                               ; e3 02                       ; 0xc1940
    rep stosb                                 ; f3 aa                       ; 0xc1942
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1944 vgabios.c:1153
    jmp short 018f3h                          ; eb aa                       ; 0xc1947
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1949 vgabios.c:1154
    pop di                                    ; 5f                          ; 0xc194c
    pop si                                    ; 5e                          ; 0xc194d
    pop bp                                    ; 5d                          ; 0xc194e
    retn 00004h                               ; c2 04 00                    ; 0xc194f
  ; disGetNextSymbol 0xc1952 LB 0x2931 -> off=0x0 cb=0000000000000082 uValue=00000000000c1952 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1952 LB 0x82
    push bp                                   ; 55                          ; 0xc1952 vgabios.c:1157
    mov bp, sp                                ; 89 e5                       ; 0xc1953
    push si                                   ; 56                          ; 0xc1955
    push di                                   ; 57                          ; 0xc1956
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1957
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc195a
    mov al, dl                                ; 88 d0                       ; 0xc195d
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc195f
    mov bx, cx                                ; 89 cb                       ; 0xc1962
    xor ah, ah                                ; 30 e4                       ; 0xc1964 vgabios.c:1163
    mov si, ax                                ; 89 c6                       ; 0xc1966
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1968
    mov di, ax                                ; 89 c7                       ; 0xc196b
    mov ax, si                                ; 89 f0                       ; 0xc196d
    imul di                                   ; f7 ef                       ; 0xc196f
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1971
    mov si, ax                                ; 89 c6                       ; 0xc1974
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1976
    xor ah, ah                                ; 30 e4                       ; 0xc1979
    mov cx, ax                                ; 89 c1                       ; 0xc197b
    add si, ax                                ; 01 c6                       ; 0xc197d
    sal si, 003h                              ; c1 e6 03                    ; 0xc197f
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1982
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1985 vgabios.c:1164
    imul di                                   ; f7 ef                       ; 0xc1988
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc198a
    add ax, cx                                ; 01 c8                       ; 0xc198d
    sal ax, 003h                              ; c1 e0 03                    ; 0xc198f
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1992
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1995 vgabios.c:1165
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc1998 vgabios.c:1166
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc199c vgabios.c:1167
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc199f
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc19a2
    jnc short 019cbh                          ; 73 24                       ; 0xc19a5
    xor ah, ah                                ; 30 e4                       ; 0xc19a7 vgabios.c:1169
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19a9
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc19ac
    add si, ax                                ; 01 c6                       ; 0xc19af
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc19b1
    add di, ax                                ; 01 c7                       ; 0xc19b4
    mov cx, bx                                ; 89 d9                       ; 0xc19b6
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc19b8
    mov es, dx                                ; 8e c2                       ; 0xc19bb
    cld                                       ; fc                          ; 0xc19bd
    jcxz 019c6h                               ; e3 06                       ; 0xc19be
    push DS                                   ; 1e                          ; 0xc19c0
    mov ds, dx                                ; 8e da                       ; 0xc19c1
    rep movsb                                 ; f3 a4                       ; 0xc19c3
    pop DS                                    ; 1f                          ; 0xc19c5
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc19c6 vgabios.c:1170
    jmp short 0199fh                          ; eb d4                       ; 0xc19c9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc19cb vgabios.c:1171
    pop di                                    ; 5f                          ; 0xc19ce
    pop si                                    ; 5e                          ; 0xc19cf
    pop bp                                    ; 5d                          ; 0xc19d0
    retn 00004h                               ; c2 04 00                    ; 0xc19d1
  ; disGetNextSymbol 0xc19d4 LB 0x28af -> off=0x0 cb=000000000000006e uValue=00000000000c19d4 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc19d4 LB 0x6e
    push bp                                   ; 55                          ; 0xc19d4 vgabios.c:1174
    mov bp, sp                                ; 89 e5                       ; 0xc19d5
    push si                                   ; 56                          ; 0xc19d7
    push di                                   ; 57                          ; 0xc19d8
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc19d9
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc19dc
    mov al, dl                                ; 88 d0                       ; 0xc19df
    mov si, cx                                ; 89 ce                       ; 0xc19e1
    xor ah, ah                                ; 30 e4                       ; 0xc19e3 vgabios.c:1180
    mov dx, ax                                ; 89 c2                       ; 0xc19e5
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc19e7
    mov di, ax                                ; 89 c7                       ; 0xc19ea
    mov ax, dx                                ; 89 d0                       ; 0xc19ec
    imul di                                   ; f7 ef                       ; 0xc19ee
    mul cx                                    ; f7 e1                       ; 0xc19f0
    mov dx, ax                                ; 89 c2                       ; 0xc19f2
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc19f4
    xor ah, ah                                ; 30 e4                       ; 0xc19f7
    add ax, dx                                ; 01 d0                       ; 0xc19f9
    sal ax, 003h                              ; c1 e0 03                    ; 0xc19fb
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc19fe
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1a01 vgabios.c:1181
    sal si, 003h                              ; c1 e6 03                    ; 0xc1a04 vgabios.c:1182
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1a07 vgabios.c:1183
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a0b
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1a0e
    jnc short 01a39h                          ; 73 26                       ; 0xc1a11
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a13 vgabios.c:1185
    xor ah, ah                                ; 30 e4                       ; 0xc1a16
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1a18
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a1b
    mul si                                    ; f7 e6                       ; 0xc1a1e
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1a20
    add di, ax                                ; 01 c7                       ; 0xc1a23
    mov cx, bx                                ; 89 d9                       ; 0xc1a25
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1a27
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a2a
    mov es, dx                                ; 8e c2                       ; 0xc1a2d
    cld                                       ; fc                          ; 0xc1a2f
    jcxz 01a34h                               ; e3 02                       ; 0xc1a30
    rep stosb                                 ; f3 aa                       ; 0xc1a32
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1a34 vgabios.c:1186
    jmp short 01a0bh                          ; eb d2                       ; 0xc1a37
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a39 vgabios.c:1187
    pop di                                    ; 5f                          ; 0xc1a3c
    pop si                                    ; 5e                          ; 0xc1a3d
    pop bp                                    ; 5d                          ; 0xc1a3e
    retn 00004h                               ; c2 04 00                    ; 0xc1a3f
  ; disGetNextSymbol 0xc1a42 LB 0x2841 -> off=0x0 cb=0000000000000690 uValue=00000000000c1a42 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1a42 LB 0x690
    push bp                                   ; 55                          ; 0xc1a42 vgabios.c:1190
    mov bp, sp                                ; 89 e5                       ; 0xc1a43
    push si                                   ; 56                          ; 0xc1a45
    push di                                   ; 57                          ; 0xc1a46
    sub sp, strict byte 0001eh                ; 83 ec 1e                    ; 0xc1a47
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a4a
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1a4d
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1a50
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1a53
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1a56 vgabios.c:1199
    jnbe short 01a77h                         ; 77 1c                       ; 0xc1a59
    cmp cl, byte [bp+006h]                    ; 3a 4e 06                    ; 0xc1a5b vgabios.c:1200
    jnbe short 01a77h                         ; 77 17                       ; 0xc1a5e
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1a60 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1a63
    mov es, ax                                ; 8e c0                       ; 0xc1a66
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a68
    xor ah, ah                                ; 30 e4                       ; 0xc1a6b vgabios.c:1204
    call 035d1h                               ; e8 61 1b                    ; 0xc1a6d
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1a70
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1a73 vgabios.c:1205
    jne short 01a7ah                          ; 75 03                       ; 0xc1a75
    jmp near 020c9h                           ; e9 4f 06                    ; 0xc1a77
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1a7a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1a7d
    mov es, ax                                ; 8e c0                       ; 0xc1a80
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a82
    xor ah, ah                                ; 30 e4                       ; 0xc1a85 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc1a87
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1a88
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1a8b vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1a8e
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1a91 vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1a94 vgabios.c:1212
    jne short 01aa3h                          ; 75 09                       ; 0xc1a98
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1a9a vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1a9d
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1aa0 vgabios.c:38
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1aa3 vgabios.c:1215
    xor ah, ah                                ; 30 e4                       ; 0xc1aa6
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1aa8
    jc short 01ab5h                           ; 72 08                       ; 0xc1aab
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1aad
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ab0
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1ab2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1ab5 vgabios.c:1216
    xor ah, ah                                ; 30 e4                       ; 0xc1ab8
    cmp ax, word [bp-01eh]                    ; 3b 46 e2                    ; 0xc1aba
    jc short 01ac7h                           ; 72 08                       ; 0xc1abd
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1abf
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ac2
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc1ac4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ac7 vgabios.c:1217
    xor ah, ah                                ; 30 e4                       ; 0xc1aca
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1acc
    jbe short 01ad4h                          ; 76 03                       ; 0xc1acf
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1ad1
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1ad4 vgabios.c:1218
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1ad7
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1ada
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1adc
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1adf vgabios.c:1220
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1ae2
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1ae5
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1ae9
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1aec
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1aef
    dec ax                                    ; 48                          ; 0xc1af2
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1af3
    mov di, word [bp-016h]                    ; 8b 7e ea                    ; 0xc1af6
    dec di                                    ; 4f                          ; 0xc1af9
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1afa
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc1afd
    mov cx, ax                                ; 89 c1                       ; 0xc1b00
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1b02
    jne short 01b53h                          ; 75 4a                       ; 0xc1b07
    add ax, ax                                ; 01 c0                       ; 0xc1b09 vgabios.c:1223
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1b0b
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1b0d
    xor dh, dh                                ; 30 f6                       ; 0xc1b10
    inc ax                                    ; 40                          ; 0xc1b12
    mul dx                                    ; f7 e2                       ; 0xc1b13
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1b15
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b18 vgabios.c:1228
    jne short 01b56h                          ; 75 38                       ; 0xc1b1c
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1b1e
    jne short 01b56h                          ; 75 32                       ; 0xc1b22
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b24
    jne short 01b56h                          ; 75 2c                       ; 0xc1b28
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b2a
    xor ah, ah                                ; 30 e4                       ; 0xc1b2d
    cmp ax, di                                ; 39 f8                       ; 0xc1b2f
    jne short 01b56h                          ; 75 23                       ; 0xc1b31
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1b33
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1b36
    jne short 01b56h                          ; 75 1b                       ; 0xc1b39
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b3b vgabios.c:1230
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1b3e
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b41
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b44
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1b48
    cld                                       ; fc                          ; 0xc1b4b
    jcxz 01b50h                               ; e3 02                       ; 0xc1b4c
    rep stosw                                 ; f3 ab                       ; 0xc1b4e
    jmp near 020c9h                           ; e9 76 05                    ; 0xc1b50 vgabios.c:1232
    jmp near 01ccah                           ; e9 74 01                    ; 0xc1b53
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1b56 vgabios.c:1234
    jne short 01bbdh                          ; 75 61                       ; 0xc1b5a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1b5c vgabios.c:1235
    xor ah, ah                                ; 30 e4                       ; 0xc1b5f
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1b61
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1b64
    xor dh, dh                                ; 30 f6                       ; 0xc1b67
    cmp dx, word [bp-01ch]                    ; 3b 56 e4                    ; 0xc1b69
    jc short 01bbfh                           ; 72 51                       ; 0xc1b6c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b6e vgabios.c:1237
    xor ah, ah                                ; 30 e4                       ; 0xc1b71
    add ax, word [bp-01ch]                    ; 03 46 e4                    ; 0xc1b73
    cmp ax, dx                                ; 39 d0                       ; 0xc1b76
    jnbe short 01b80h                         ; 77 06                       ; 0xc1b78
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b7a
    jne short 01bc2h                          ; 75 42                       ; 0xc1b7e
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1b80 vgabios.c:1238
    xor ch, ch                                ; 30 ed                       ; 0xc1b83
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b85
    xor ah, ah                                ; 30 e4                       ; 0xc1b88
    mov si, ax                                ; 89 c6                       ; 0xc1b8a
    sal si, 008h                              ; c1 e6 08                    ; 0xc1b8c
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1b8f
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1b92
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1b95
    mov dx, ax                                ; 89 c2                       ; 0xc1b98
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b9a
    xor ah, ah                                ; 30 e4                       ; 0xc1b9d
    mov di, ax                                ; 89 c7                       ; 0xc1b9f
    add di, dx                                ; 01 d7                       ; 0xc1ba1
    add di, di                                ; 01 ff                       ; 0xc1ba3
    add di, word [bp-020h]                    ; 03 7e e0                    ; 0xc1ba5
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1ba8
    xor bh, bh                                ; 30 ff                       ; 0xc1bab
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1bad
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1bb0
    mov ax, si                                ; 89 f0                       ; 0xc1bb4
    cld                                       ; fc                          ; 0xc1bb6
    jcxz 01bbbh                               ; e3 02                       ; 0xc1bb7
    rep stosw                                 ; f3 ab                       ; 0xc1bb9
    jmp short 01c03h                          ; eb 46                       ; 0xc1bbb vgabios.c:1239
    jmp short 01c09h                          ; eb 4a                       ; 0xc1bbd
    jmp near 020c9h                           ; e9 07 05                    ; 0xc1bbf
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1bc2 vgabios.c:1240
    xor ch, ch                                ; 30 ed                       ; 0xc1bc5
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1bc7
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1bca
    mov byte [bp-018h], dl                    ; 88 56 e8                    ; 0xc1bcd
    mov byte [bp-017h], ch                    ; 88 6e e9                    ; 0xc1bd0
    mov si, ax                                ; 89 c6                       ; 0xc1bd3
    add si, word [bp-018h]                    ; 03 76 e8                    ; 0xc1bd5
    add si, si                                ; 01 f6                       ; 0xc1bd8
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1bda
    xor bh, bh                                ; 30 ff                       ; 0xc1bdd
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1bdf
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1be2
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1be6
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1be9
    add ax, word [bp-018h]                    ; 03 46 e8                    ; 0xc1bec
    add ax, ax                                ; 01 c0                       ; 0xc1bef
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1bf1
    add di, ax                                ; 01 c7                       ; 0xc1bf4
    mov dx, bx                                ; 89 da                       ; 0xc1bf6
    mov es, bx                                ; 8e c3                       ; 0xc1bf8
    cld                                       ; fc                          ; 0xc1bfa
    jcxz 01c03h                               ; e3 06                       ; 0xc1bfb
    push DS                                   ; 1e                          ; 0xc1bfd
    mov ds, dx                                ; 8e da                       ; 0xc1bfe
    rep movsw                                 ; f3 a5                       ; 0xc1c00
    pop DS                                    ; 1f                          ; 0xc1c02
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1c03 vgabios.c:1241
    jmp near 01b64h                           ; e9 5b ff                    ; 0xc1c06
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c09 vgabios.c:1244
    xor ah, ah                                ; 30 e4                       ; 0xc1c0c
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1c0e
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1c11
    xor ah, ah                                ; 30 e4                       ; 0xc1c14
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1c16
    jnbe short 01bbfh                         ; 77 a4                       ; 0xc1c19
    mov dl, al                                ; 88 c2                       ; 0xc1c1b vgabios.c:1246
    xor dh, dh                                ; 30 f6                       ; 0xc1c1d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c1f
    add ax, dx                                ; 01 d0                       ; 0xc1c22
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1c24
    jnbe short 01c2fh                         ; 77 06                       ; 0xc1c27
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1c29
    jne short 01c6ch                          ; 75 3d                       ; 0xc1c2d
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1c2f vgabios.c:1247
    xor ch, ch                                ; 30 ed                       ; 0xc1c32
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1c34
    xor ah, ah                                ; 30 e4                       ; 0xc1c37
    mov si, ax                                ; 89 c6                       ; 0xc1c39
    sal si, 008h                              ; c1 e6 08                    ; 0xc1c3b
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1c3e
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1c41
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c44
    mov dx, ax                                ; 89 c2                       ; 0xc1c47
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1c49
    xor ah, ah                                ; 30 e4                       ; 0xc1c4c
    add ax, dx                                ; 01 d0                       ; 0xc1c4e
    add ax, ax                                ; 01 c0                       ; 0xc1c50
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1c52
    add di, ax                                ; 01 c7                       ; 0xc1c55
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c57
    xor bh, bh                                ; 30 ff                       ; 0xc1c5a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c5c
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1c5f
    mov ax, si                                ; 89 f0                       ; 0xc1c63
    cld                                       ; fc                          ; 0xc1c65
    jcxz 01c6ah                               ; e3 02                       ; 0xc1c66
    rep stosw                                 ; f3 ab                       ; 0xc1c68
    jmp short 01cbah                          ; eb 4e                       ; 0xc1c6a vgabios.c:1248
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1c6c vgabios.c:1249
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1c6f
    mov byte [bp-017h], dh                    ; 88 76 e9                    ; 0xc1c72
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c75
    xor ah, ah                                ; 30 e4                       ; 0xc1c78
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1c7a
    sub dx, ax                                ; 29 c2                       ; 0xc1c7d
    mov ax, dx                                ; 89 d0                       ; 0xc1c7f
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c81
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc1c84
    xor ch, ch                                ; 30 ed                       ; 0xc1c87
    mov si, ax                                ; 89 c6                       ; 0xc1c89
    add si, cx                                ; 01 ce                       ; 0xc1c8b
    add si, si                                ; 01 f6                       ; 0xc1c8d
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c8f
    xor bh, bh                                ; 30 ff                       ; 0xc1c92
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c94
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1c97
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1c9b
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1c9e
    add ax, cx                                ; 01 c8                       ; 0xc1ca1
    add ax, ax                                ; 01 c0                       ; 0xc1ca3
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1ca5
    add di, ax                                ; 01 c7                       ; 0xc1ca8
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc1caa
    mov dx, bx                                ; 89 da                       ; 0xc1cad
    mov es, bx                                ; 8e c3                       ; 0xc1caf
    cld                                       ; fc                          ; 0xc1cb1
    jcxz 01cbah                               ; e3 06                       ; 0xc1cb2
    push DS                                   ; 1e                          ; 0xc1cb4
    mov ds, dx                                ; 8e da                       ; 0xc1cb5
    rep movsw                                 ; f3 a5                       ; 0xc1cb7
    pop DS                                    ; 1f                          ; 0xc1cb9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1cba vgabios.c:1250
    xor ah, ah                                ; 30 e4                       ; 0xc1cbd
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1cbf
    jc short 01cf7h                           ; 72 33                       ; 0xc1cc2
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1cc4 vgabios.c:1251
    jmp near 01c11h                           ; e9 47 ff                    ; 0xc1cc7
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1cca vgabios.c:1257
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1ccd
    xor ah, ah                                ; 30 e4                       ; 0xc1cd1
    mov si, ax                                ; 89 c6                       ; 0xc1cd3
    sal si, 006h                              ; c1 e6 06                    ; 0xc1cd5
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1cd8
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1cdc
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1cdf vgabios.c:1258
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1ce3
    jc short 01cf3h                           ; 72 0c                       ; 0xc1ce5
    jbe short 01cfah                          ; 76 11                       ; 0xc1ce7
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1ce9
    je short 01d28h                           ; 74 3b                       ; 0xc1ceb
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1ced
    je short 01cfah                           ; 74 09                       ; 0xc1cef
    jmp short 01cf7h                          ; eb 04                       ; 0xc1cf1
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1cf3
    je short 01d2bh                           ; 74 34                       ; 0xc1cf5
    jmp near 020c9h                           ; e9 cf 03                    ; 0xc1cf7
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1cfa vgabios.c:1262
    jne short 01d26h                          ; 75 26                       ; 0xc1cfe
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d00
    jne short 01d69h                          ; 75 63                       ; 0xc1d04
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d06
    jne short 01d69h                          ; 75 5d                       ; 0xc1d0a
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d0c
    xor ah, ah                                ; 30 e4                       ; 0xc1d0f
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1d11
    dec dx                                    ; 4a                          ; 0xc1d14
    cmp ax, dx                                ; 39 d0                       ; 0xc1d15
    jne short 01d69h                          ; 75 50                       ; 0xc1d17
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1d19
    xor ah, dh                                ; 30 f4                       ; 0xc1d1c
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc1d1e
    dec dx                                    ; 4a                          ; 0xc1d21
    cmp ax, dx                                ; 39 d0                       ; 0xc1d22
    je short 01d2eh                           ; 74 08                       ; 0xc1d24
    jmp short 01d69h                          ; eb 41                       ; 0xc1d26
    jmp near 01fa0h                           ; e9 75 02                    ; 0xc1d28
    jmp near 01e59h                           ; e9 2b 01                    ; 0xc1d2b
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1d2e vgabios.c:1264
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d31
    out DX, ax                                ; ef                          ; 0xc1d34
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1d35 vgabios.c:1265
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1d38
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1d3b
    xor dh, dh                                ; 30 f6                       ; 0xc1d3e
    mul dx                                    ; f7 e2                       ; 0xc1d40
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc1d42
    xor dh, dh                                ; 30 f6                       ; 0xc1d45
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1d47
    xor bh, bh                                ; 30 ff                       ; 0xc1d4a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1d4c
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1d4f
    mov cx, ax                                ; 89 c1                       ; 0xc1d53
    mov ax, dx                                ; 89 d0                       ; 0xc1d55
    xor di, di                                ; 31 ff                       ; 0xc1d57
    mov es, bx                                ; 8e c3                       ; 0xc1d59
    cld                                       ; fc                          ; 0xc1d5b
    jcxz 01d60h                               ; e3 02                       ; 0xc1d5c
    rep stosb                                 ; f3 aa                       ; 0xc1d5e
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1d60 vgabios.c:1266
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d63
    out DX, ax                                ; ef                          ; 0xc1d66
    jmp short 01cf7h                          ; eb 8e                       ; 0xc1d67 vgabios.c:1268
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d69 vgabios.c:1270
    jne short 01de4h                          ; 75 75                       ; 0xc1d6d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1d6f vgabios.c:1271
    xor ah, ah                                ; 30 e4                       ; 0xc1d72
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1d74
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d77
    xor ah, ah                                ; 30 e4                       ; 0xc1d7a
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1d7c
    jc short 01de1h                           ; 72 60                       ; 0xc1d7f
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1d81 vgabios.c:1273
    xor dh, dh                                ; 30 f6                       ; 0xc1d84
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1d86
    cmp dx, ax                                ; 39 c2                       ; 0xc1d89
    jnbe short 01d93h                         ; 77 06                       ; 0xc1d8b
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d8d
    jne short 01db4h                          ; 75 21                       ; 0xc1d91
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1d93 vgabios.c:1274
    xor ah, ah                                ; 30 e4                       ; 0xc1d96
    push ax                                   ; 50                          ; 0xc1d98
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1d99
    push ax                                   ; 50                          ; 0xc1d9c
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1d9d
    xor ch, ch                                ; 30 ed                       ; 0xc1da0
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1da2
    xor bh, bh                                ; 30 ff                       ; 0xc1da5
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1da7
    xor dh, dh                                ; 30 f6                       ; 0xc1daa
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1dac
    call 01788h                               ; e8 d6 f9                    ; 0xc1daf
    jmp short 01ddch                          ; eb 28                       ; 0xc1db2 vgabios.c:1275
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1db4 vgabios.c:1276
    push ax                                   ; 50                          ; 0xc1db7
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1db8
    push ax                                   ; 50                          ; 0xc1dbb
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1dbc
    xor ch, ch                                ; 30 ed                       ; 0xc1dbf
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1dc1
    xor bh, bh                                ; 30 ff                       ; 0xc1dc4
    mov dl, bl                                ; 88 da                       ; 0xc1dc6
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1dc8
    xor dh, dh                                ; 30 f6                       ; 0xc1dcb
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1dcd
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1dd0
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc1dd3
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1dd6
    call 016f9h                               ; e8 1d f9                    ; 0xc1dd9
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1ddc vgabios.c:1277
    jmp short 01d77h                          ; eb 96                       ; 0xc1ddf
    jmp near 020c9h                           ; e9 e5 02                    ; 0xc1de1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1de4 vgabios.c:1280
    xor ah, ah                                ; 30 e4                       ; 0xc1de7
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1de9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1dec
    xor ah, ah                                ; 30 e4                       ; 0xc1def
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1df1
    jnbe short 01de1h                         ; 77 eb                       ; 0xc1df4
    mov dl, al                                ; 88 c2                       ; 0xc1df6 vgabios.c:1282
    xor dh, dh                                ; 30 f6                       ; 0xc1df8
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1dfa
    add ax, dx                                ; 01 d0                       ; 0xc1dfd
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1dff
    jnbe short 01e0ah                         ; 77 06                       ; 0xc1e02
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1e04
    jne short 01e2bh                          ; 75 21                       ; 0xc1e08
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e0a vgabios.c:1283
    xor ah, ah                                ; 30 e4                       ; 0xc1e0d
    push ax                                   ; 50                          ; 0xc1e0f
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e10
    push ax                                   ; 50                          ; 0xc1e13
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1e14
    xor ch, ch                                ; 30 ed                       ; 0xc1e17
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e19
    xor bh, bh                                ; 30 ff                       ; 0xc1e1c
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1e1e
    xor dh, dh                                ; 30 f6                       ; 0xc1e21
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e23
    call 01788h                               ; e8 5f f9                    ; 0xc1e26
    jmp short 01e4ah                          ; eb 1f                       ; 0xc1e29 vgabios.c:1284
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e2b vgabios.c:1285
    xor ah, ah                                ; 30 e4                       ; 0xc1e2e
    push ax                                   ; 50                          ; 0xc1e30
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1e31
    push ax                                   ; 50                          ; 0xc1e34
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e35
    xor ch, ch                                ; 30 ed                       ; 0xc1e38
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1e3a
    xor bh, bh                                ; 30 ff                       ; 0xc1e3d
    mov dl, bl                                ; 88 da                       ; 0xc1e3f
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1e41
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e44
    call 016f9h                               ; e8 af f8                    ; 0xc1e47
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e4a vgabios.c:1286
    xor ah, ah                                ; 30 e4                       ; 0xc1e4d
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1e4f
    jc short 01ea3h                           ; 72 4f                       ; 0xc1e52
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1e54 vgabios.c:1287
    jmp short 01dech                          ; eb 93                       ; 0xc1e57
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc1e59 vgabios.c:1292
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1e5d
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1e60 vgabios.c:1293
    jne short 01ea6h                          ; 75 40                       ; 0xc1e64
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1e66
    jne short 01ea6h                          ; 75 3a                       ; 0xc1e6a
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1e6c
    jne short 01ea6h                          ; 75 34                       ; 0xc1e70
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e72
    cmp ax, di                                ; 39 f8                       ; 0xc1e75
    jne short 01ea6h                          ; 75 2d                       ; 0xc1e77
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1e79
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1e7c
    jne short 01ea6h                          ; 75 25                       ; 0xc1e7f
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1e81 vgabios.c:1295
    xor dh, dh                                ; 30 f6                       ; 0xc1e84
    mov ax, cx                                ; 89 c8                       ; 0xc1e86
    mul dx                                    ; f7 e2                       ; 0xc1e88
    mov dl, byte [bp-014h]                    ; 8a 56 ec                    ; 0xc1e8a
    xor dh, dh                                ; 30 f6                       ; 0xc1e8d
    mul dx                                    ; f7 e2                       ; 0xc1e8f
    mov cx, ax                                ; 89 c1                       ; 0xc1e91
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e93
    xor ah, ah                                ; 30 e4                       ; 0xc1e96
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1e98
    xor di, di                                ; 31 ff                       ; 0xc1e9c
    cld                                       ; fc                          ; 0xc1e9e
    jcxz 01ea3h                               ; e3 02                       ; 0xc1e9f
    rep stosb                                 ; f3 aa                       ; 0xc1ea1
    jmp near 020c9h                           ; e9 23 02                    ; 0xc1ea3 vgabios.c:1297
    cmp byte [bp-014h], 002h                  ; 80 7e ec 02                 ; 0xc1ea6 vgabios.c:1299
    jne short 01eb5h                          ; 75 09                       ; 0xc1eaa
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc1eac vgabios.c:1301
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1eaf vgabios.c:1302
    sal word [bp-01eh], 1                     ; d1 66 e2                    ; 0xc1eb2 vgabios.c:1303
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1eb5 vgabios.c:1306
    jne short 01f24h                          ; 75 69                       ; 0xc1eb9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1ebb vgabios.c:1307
    xor ah, ah                                ; 30 e4                       ; 0xc1ebe
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1ec0
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ec3
    xor ah, ah                                ; 30 e4                       ; 0xc1ec6
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1ec8
    jc short 01ea3h                           ; 72 d6                       ; 0xc1ecb
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1ecd vgabios.c:1309
    xor dh, dh                                ; 30 f6                       ; 0xc1ed0
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1ed2
    cmp dx, ax                                ; 39 c2                       ; 0xc1ed5
    jnbe short 01edfh                         ; 77 06                       ; 0xc1ed7
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ed9
    jne short 01f00h                          ; 75 21                       ; 0xc1edd
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1edf vgabios.c:1310
    xor ah, ah                                ; 30 e4                       ; 0xc1ee2
    push ax                                   ; 50                          ; 0xc1ee4
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1ee5
    push ax                                   ; 50                          ; 0xc1ee8
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1ee9
    xor ch, ch                                ; 30 ed                       ; 0xc1eec
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1eee
    xor bh, bh                                ; 30 ff                       ; 0xc1ef1
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1ef3
    xor dh, dh                                ; 30 f6                       ; 0xc1ef6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ef8
    call 018bch                               ; e8 be f9                    ; 0xc1efb
    jmp short 01f1fh                          ; eb 1f                       ; 0xc1efe vgabios.c:1311
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f00 vgabios.c:1312
    push ax                                   ; 50                          ; 0xc1f03
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1f04
    push ax                                   ; 50                          ; 0xc1f07
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f08
    xor ch, ch                                ; 30 ed                       ; 0xc1f0b
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1f0d
    xor bh, bh                                ; 30 ff                       ; 0xc1f10
    mov dl, bl                                ; 88 da                       ; 0xc1f12
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1f14
    xor dh, dh                                ; 30 f6                       ; 0xc1f17
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f19
    call 01804h                               ; e8 e5 f8                    ; 0xc1f1c
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1f1f vgabios.c:1313
    jmp short 01ec3h                          ; eb 9f                       ; 0xc1f22
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f24 vgabios.c:1316
    xor ah, ah                                ; 30 e4                       ; 0xc1f27
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1f29
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f2c
    xor ah, ah                                ; 30 e4                       ; 0xc1f2f
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f31
    jnbe short 01f9eh                         ; 77 68                       ; 0xc1f34
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1f36 vgabios.c:1318
    xor dh, dh                                ; 30 f6                       ; 0xc1f39
    add ax, dx                                ; 01 d0                       ; 0xc1f3b
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f3d
    jnbe short 01f46h                         ; 77 04                       ; 0xc1f40
    test dl, dl                               ; 84 d2                       ; 0xc1f42
    jne short 01f70h                          ; 75 2a                       ; 0xc1f44
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f46 vgabios.c:1319
    xor ah, ah                                ; 30 e4                       ; 0xc1f49
    push ax                                   ; 50                          ; 0xc1f4b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f4c
    push ax                                   ; 50                          ; 0xc1f4f
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1f50
    xor ch, ch                                ; 30 ed                       ; 0xc1f53
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1f55
    xor bh, bh                                ; 30 ff                       ; 0xc1f58
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1f5a
    xor dh, dh                                ; 30 f6                       ; 0xc1f5d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f5f
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1f62
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc1f65
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1f68
    call 018bch                               ; e8 4e f9                    ; 0xc1f6b
    jmp short 01f8fh                          ; eb 1f                       ; 0xc1f6e vgabios.c:1320
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f70 vgabios.c:1321
    xor ah, ah                                ; 30 e4                       ; 0xc1f73
    push ax                                   ; 50                          ; 0xc1f75
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1f76
    push ax                                   ; 50                          ; 0xc1f79
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f7a
    xor ch, ch                                ; 30 ed                       ; 0xc1f7d
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1f7f
    xor bh, bh                                ; 30 ff                       ; 0xc1f82
    mov dl, bl                                ; 88 da                       ; 0xc1f84
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1f86
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f89
    call 01804h                               ; e8 75 f8                    ; 0xc1f8c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f8f vgabios.c:1322
    xor ah, ah                                ; 30 e4                       ; 0xc1f92
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f94
    jc short 01fdfh                           ; 72 46                       ; 0xc1f97
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1f99 vgabios.c:1323
    jmp short 01f2ch                          ; eb 8e                       ; 0xc1f9c
    jmp short 01fdfh                          ; eb 3f                       ; 0xc1f9e
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1fa0 vgabios.c:1328
    jne short 01fe2h                          ; 75 3c                       ; 0xc1fa4
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1fa6
    jne short 01fe2h                          ; 75 36                       ; 0xc1faa
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1fac
    jne short 01fe2h                          ; 75 30                       ; 0xc1fb0
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1fb2
    cmp ax, di                                ; 39 f8                       ; 0xc1fb5
    jne short 01fe2h                          ; 75 29                       ; 0xc1fb7
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1fb9
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1fbc
    jne short 01fe2h                          ; 75 21                       ; 0xc1fbf
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1fc1 vgabios.c:1330
    xor dh, dh                                ; 30 f6                       ; 0xc1fc4
    mov ax, cx                                ; 89 c8                       ; 0xc1fc6
    mul dx                                    ; f7 e2                       ; 0xc1fc8
    mov cx, ax                                ; 89 c1                       ; 0xc1fca
    sal cx, 003h                              ; c1 e1 03                    ; 0xc1fcc
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1fcf
    xor ah, ah                                ; 30 e4                       ; 0xc1fd2
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1fd4
    xor di, di                                ; 31 ff                       ; 0xc1fd8
    cld                                       ; fc                          ; 0xc1fda
    jcxz 01fdfh                               ; e3 02                       ; 0xc1fdb
    rep stosb                                 ; f3 aa                       ; 0xc1fdd
    jmp near 020c9h                           ; e9 e7 00                    ; 0xc1fdf vgabios.c:1332
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1fe2 vgabios.c:1335
    jne short 02057h                          ; 75 6f                       ; 0xc1fe6
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1fe8 vgabios.c:1336
    xor ah, ah                                ; 30 e4                       ; 0xc1feb
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1fed
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ff0
    xor ah, ah                                ; 30 e4                       ; 0xc1ff3
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1ff5
    jc short 01fdfh                           ; 72 e5                       ; 0xc1ff8
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1ffa vgabios.c:1338
    xor dh, dh                                ; 30 f6                       ; 0xc1ffd
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1fff
    cmp dx, ax                                ; 39 c2                       ; 0xc2002
    jnbe short 0200ch                         ; 77 06                       ; 0xc2004
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2006
    jne short 0202bh                          ; 75 1f                       ; 0xc200a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc200c vgabios.c:1339
    xor ah, ah                                ; 30 e4                       ; 0xc200f
    push ax                                   ; 50                          ; 0xc2011
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2012
    push ax                                   ; 50                          ; 0xc2015
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2016
    xor bh, bh                                ; 30 ff                       ; 0xc2019
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc201b
    xor dh, dh                                ; 30 f6                       ; 0xc201e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2020
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc2023
    call 019d4h                               ; e8 ab f9                    ; 0xc2026
    jmp short 02052h                          ; eb 27                       ; 0xc2029 vgabios.c:1340
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc202b vgabios.c:1341
    push ax                                   ; 50                          ; 0xc202e
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc202f
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2032
    xor ch, ch                                ; 30 ed                       ; 0xc2035
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2037
    xor bh, bh                                ; 30 ff                       ; 0xc203a
    mov dl, bl                                ; 88 da                       ; 0xc203c
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc203e
    xor dh, dh                                ; 30 f6                       ; 0xc2041
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2043
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc2046
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc2049
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc204c
    call 01952h                               ; e8 00 f9                    ; 0xc204f
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc2052 vgabios.c:1342
    jmp short 01ff0h                          ; eb 99                       ; 0xc2055
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2057 vgabios.c:1345
    xor ah, ah                                ; 30 e4                       ; 0xc205a
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc205c
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc205f
    xor ah, ah                                ; 30 e4                       ; 0xc2062
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc2064
    jnbe short 020c9h                         ; 77 60                       ; 0xc2067
    mov dl, al                                ; 88 c2                       ; 0xc2069 vgabios.c:1347
    xor dh, dh                                ; 30 f6                       ; 0xc206b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc206d
    add ax, dx                                ; 01 d0                       ; 0xc2070
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc2072
    jnbe short 0207dh                         ; 77 06                       ; 0xc2075
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2077
    jne short 0209ch                          ; 75 1f                       ; 0xc207b
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc207d vgabios.c:1348
    xor ah, ah                                ; 30 e4                       ; 0xc2080
    push ax                                   ; 50                          ; 0xc2082
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2083
    push ax                                   ; 50                          ; 0xc2086
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2087
    xor bh, bh                                ; 30 ff                       ; 0xc208a
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc208c
    xor dh, dh                                ; 30 f6                       ; 0xc208f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2091
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc2094
    call 019d4h                               ; e8 3a f9                    ; 0xc2097
    jmp short 020bah                          ; eb 1e                       ; 0xc209a vgabios.c:1349
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc209c vgabios.c:1350
    xor ah, ah                                ; 30 e4                       ; 0xc209f
    push ax                                   ; 50                          ; 0xc20a1
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc20a2
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc20a5
    xor ch, ch                                ; 30 ed                       ; 0xc20a8
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc20aa
    xor bh, bh                                ; 30 ff                       ; 0xc20ad
    mov dl, bl                                ; 88 da                       ; 0xc20af
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc20b1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20b4
    call 01952h                               ; e8 98 f8                    ; 0xc20b7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20ba vgabios.c:1351
    xor ah, ah                                ; 30 e4                       ; 0xc20bd
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc20bf
    jc short 020c9h                           ; 72 05                       ; 0xc20c2
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc20c4 vgabios.c:1352
    jmp short 0205fh                          ; eb 96                       ; 0xc20c7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc20c9 vgabios.c:1363
    pop di                                    ; 5f                          ; 0xc20cc
    pop si                                    ; 5e                          ; 0xc20cd
    pop bp                                    ; 5d                          ; 0xc20ce
    retn 00008h                               ; c2 08 00                    ; 0xc20cf
  ; disGetNextSymbol 0xc20d2 LB 0x21b1 -> off=0x0 cb=0000000000000111 uValue=00000000000c20d2 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc20d2 LB 0x111
    push bp                                   ; 55                          ; 0xc20d2 vgabios.c:1366
    mov bp, sp                                ; 89 e5                       ; 0xc20d3
    push si                                   ; 56                          ; 0xc20d5
    push di                                   ; 57                          ; 0xc20d6
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc20d7
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc20da
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc20dd
    mov ch, bl                                ; 88 dd                       ; 0xc20e0
    mov al, cl                                ; 88 c8                       ; 0xc20e2
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc20e4 vgabios.c:57
    xor dx, dx                                ; 31 d2                       ; 0xc20e7
    mov es, dx                                ; 8e c2                       ; 0xc20e9
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc20eb
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc20ee
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc20f2 vgabios.c:58
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc20f5
    xor ah, ah                                ; 30 e4                       ; 0xc20f8 vgabios.c:1375
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc20fa
    xor bh, bh                                ; 30 ff                       ; 0xc20fd
    imul bx                                   ; f7 eb                       ; 0xc20ff
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc2101
    xor dh, dh                                ; 30 f6                       ; 0xc2104
    imul dx                                   ; f7 ea                       ; 0xc2106
    mov si, ax                                ; 89 c6                       ; 0xc2108
    mov al, ch                                ; 88 e8                       ; 0xc210a
    xor ah, ah                                ; 30 e4                       ; 0xc210c
    add si, ax                                ; 01 c6                       ; 0xc210e
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc2110 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2113
    mov es, ax                                ; 8e c0                       ; 0xc2116
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc2118
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc211b vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc211e
    mul dx                                    ; f7 e2                       ; 0xc2120
    add si, ax                                ; 01 c6                       ; 0xc2122
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2124 vgabios.c:1377
    xor ah, ah                                ; 30 e4                       ; 0xc2127
    imul bx                                   ; f7 eb                       ; 0xc2129
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc212b
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc212e vgabios.c:1378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2131
    out DX, ax                                ; ef                          ; 0xc2134
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2135 vgabios.c:1379
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2138
    out DX, ax                                ; ef                          ; 0xc213b
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc213c vgabios.c:1380
    je short 02148h                           ; 74 06                       ; 0xc2140
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2142 vgabios.c:1382
    out DX, ax                                ; ef                          ; 0xc2145
    jmp short 0214ch                          ; eb 04                       ; 0xc2146 vgabios.c:1384
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2148 vgabios.c:1386
    out DX, ax                                ; ef                          ; 0xc214b
    xor ch, ch                                ; 30 ed                       ; 0xc214c vgabios.c:1388
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc214e
    jnc short 021c5h                          ; 73 72                       ; 0xc2151
    mov al, ch                                ; 88 e8                       ; 0xc2153 vgabios.c:1390
    xor ah, ah                                ; 30 e4                       ; 0xc2155
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2157
    xor bh, bh                                ; 30 ff                       ; 0xc215a
    imul bx                                   ; f7 eb                       ; 0xc215c
    mov bx, si                                ; 89 f3                       ; 0xc215e
    add bx, ax                                ; 01 c3                       ; 0xc2160
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc2162 vgabios.c:1391
    jmp short 0217ah                          ; eb 12                       ; 0xc2166
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2168 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc216b
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc216d
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2171 vgabios.c:1404
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc2174
    jnc short 021c7h                          ; 73 4d                       ; 0xc2178
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc217a
    mov ax, 00080h                            ; b8 80 00                    ; 0xc217d
    sar ax, CL                                ; d3 f8                       ; 0xc2180
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2182
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc2185
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2189
    sal ax, 008h                              ; c1 e0 08                    ; 0xc218c
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc218f
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2191
    out DX, ax                                ; ef                          ; 0xc2194
    mov dx, bx                                ; 89 da                       ; 0xc2195
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2197
    call 035f9h                               ; e8 5c 14                    ; 0xc219a
    mov al, ch                                ; 88 e8                       ; 0xc219d
    xor ah, ah                                ; 30 e4                       ; 0xc219f
    add ax, word [bp-010h]                    ; 03 46 f0                    ; 0xc21a1
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc21a4
    mov di, word [bp-012h]                    ; 8b 7e ee                    ; 0xc21a7
    add di, ax                                ; 01 c7                       ; 0xc21aa
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc21ac
    xor ah, ah                                ; 30 e4                       ; 0xc21af
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc21b1
    je short 02168h                           ; 74 b2                       ; 0xc21b4
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc21b6
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc21b9
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc21bb
    mov es, dx                                ; 8e c2                       ; 0xc21be
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc21c0
    jmp short 02171h                          ; eb ac                       ; 0xc21c3
    jmp short 021cbh                          ; eb 04                       ; 0xc21c5
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc21c7 vgabios.c:1405
    jmp short 0214eh                          ; eb 83                       ; 0xc21c9
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc21cb vgabios.c:1406
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21ce
    out DX, ax                                ; ef                          ; 0xc21d1
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc21d2 vgabios.c:1407
    out DX, ax                                ; ef                          ; 0xc21d5
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc21d6 vgabios.c:1408
    out DX, ax                                ; ef                          ; 0xc21d9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc21da vgabios.c:1409
    pop di                                    ; 5f                          ; 0xc21dd
    pop si                                    ; 5e                          ; 0xc21de
    pop bp                                    ; 5d                          ; 0xc21df
    retn 00006h                               ; c2 06 00                    ; 0xc21e0
  ; disGetNextSymbol 0xc21e3 LB 0x20a0 -> off=0x0 cb=0000000000000112 uValue=00000000000c21e3 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc21e3 LB 0x112
    push si                                   ; 56                          ; 0xc21e3 vgabios.c:1412
    push di                                   ; 57                          ; 0xc21e4
    enter 0000ch, 000h                        ; c8 0c 00 00                 ; 0xc21e5
    mov bh, al                                ; 88 c7                       ; 0xc21e9
    mov ch, dl                                ; 88 d5                       ; 0xc21eb
    mov al, bl                                ; 88 d8                       ; 0xc21ed
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc21ef vgabios.c:1419
    xor ah, ah                                ; 30 e4                       ; 0xc21f2 vgabios.c:1420
    mov dl, byte [bp+00ah]                    ; 8a 56 0a                    ; 0xc21f4
    xor dh, dh                                ; 30 f6                       ; 0xc21f7
    imul dx                                   ; f7 ea                       ; 0xc21f9
    mov dl, cl                                ; 88 ca                       ; 0xc21fb
    xor dh, dh                                ; 30 f6                       ; 0xc21fd
    imul dx, dx, 00140h                       ; 69 d2 40 01                 ; 0xc21ff
    add ax, dx                                ; 01 d0                       ; 0xc2203
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2205
    mov al, bh                                ; 88 f8                       ; 0xc2208 vgabios.c:1421
    xor ah, ah                                ; 30 e4                       ; 0xc220a
    sal ax, 003h                              ; c1 e0 03                    ; 0xc220c
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc220f
    xor ah, ah                                ; 30 e4                       ; 0xc2212 vgabios.c:1422
    jmp near 02233h                           ; e9 1c 00                    ; 0xc2214
    mov dl, ah                                ; 88 e2                       ; 0xc2217 vgabios.c:1437
    xor dh, dh                                ; 30 f6                       ; 0xc2219
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc221b
    mov si, di                                ; 89 fe                       ; 0xc221e
    add si, dx                                ; 01 d6                       ; 0xc2220
    mov al, byte [si]                         ; 8a 04                       ; 0xc2222
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2224 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc2227
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2229
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc222c vgabios.c:1441
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc222e
    jnc short 0228ah                          ; 73 57                       ; 0xc2231
    mov dl, ah                                ; 88 e2                       ; 0xc2233
    xor dh, dh                                ; 30 f6                       ; 0xc2235
    sar dx, 1                                 ; d1 fa                       ; 0xc2237
    imul dx, dx, strict byte 00050h           ; 6b d2 50                    ; 0xc2239
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc223c
    add bx, dx                                ; 01 d3                       ; 0xc223f
    test ah, 001h                             ; f6 c4 01                    ; 0xc2241
    je short 02249h                           ; 74 03                       ; 0xc2244
    add bh, 020h                              ; 80 c7 20                    ; 0xc2246
    mov byte [bp-002h], 080h                  ; c6 46 fe 80                 ; 0xc2249
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc224d
    jne short 0226fh                          ; 75 1c                       ; 0xc2251
    test ch, 080h                             ; f6 c5 80                    ; 0xc2253
    je short 02217h                           ; 74 bf                       ; 0xc2256
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2258
    mov es, dx                                ; 8e c2                       ; 0xc225b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc225d
    mov dl, ah                                ; 88 e2                       ; 0xc2260
    xor dh, dh                                ; 30 f6                       ; 0xc2262
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc2264
    mov si, di                                ; 89 fe                       ; 0xc2267
    add si, dx                                ; 01 d6                       ; 0xc2269
    xor al, byte [si]                         ; 32 04                       ; 0xc226b
    jmp short 02224h                          ; eb b5                       ; 0xc226d
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00                 ; 0xc226f vgabios.c:1443
    jbe short 0222ch                          ; 76 b7                       ; 0xc2273
    test ch, 080h                             ; f6 c5 80                    ; 0xc2275 vgabios.c:1445
    je short 02284h                           ; 74 0a                       ; 0xc2278
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc227a vgabios.c:37
    mov es, dx                                ; 8e c2                       ; 0xc227d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc227f
    jmp short 02286h                          ; eb 02                       ; 0xc2282 vgabios.c:1449
    xor al, al                                ; 30 c0                       ; 0xc2284 vgabios.c:1451
    xor dl, dl                                ; 30 d2                       ; 0xc2286 vgabios.c:1453
    jmp short 02291h                          ; eb 07                       ; 0xc2288
    jmp short 022efh                          ; eb 63                       ; 0xc228a
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc228c
    jnc short 022e4h                          ; 73 53                       ; 0xc228f
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2291 vgabios.c:1455
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc2294
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc2298
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc229b
    add si, di                                ; 01 fe                       ; 0xc229e
    mov dh, byte [si]                         ; 8a 34                       ; 0xc22a0
    mov byte [bp-006h], dh                    ; 88 76 fa                    ; 0xc22a2
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc22a5
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc22a9
    mov byte [bp-00ah], dh                    ; 88 76 f6                    ; 0xc22ac
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc22af
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc22b3
    test word [bp-00ah], si                   ; 85 76 f6                    ; 0xc22b6
    je short 022ddh                           ; 74 22                       ; 0xc22b9
    mov DH, strict byte 003h                  ; b6 03                       ; 0xc22bb vgabios.c:1456
    sub dh, dl                                ; 28 d6                       ; 0xc22bd
    mov cl, ch                                ; 88 e9                       ; 0xc22bf
    and cl, 003h                              ; 80 e1 03                    ; 0xc22c1
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc22c4
    mov cl, dh                                ; 88 f1                       ; 0xc22c7
    add cl, dh                                ; 00 f1                       ; 0xc22c9
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc22cb
    sal dh, CL                                ; d2 e6                       ; 0xc22ce
    mov cl, dh                                ; 88 f1                       ; 0xc22d0
    test ch, 080h                             ; f6 c5 80                    ; 0xc22d2 vgabios.c:1457
    je short 022dbh                           ; 74 04                       ; 0xc22d5
    xor al, dh                                ; 30 f0                       ; 0xc22d7 vgabios.c:1459
    jmp short 022ddh                          ; eb 02                       ; 0xc22d9 vgabios.c:1461
    or al, dh                                 ; 08 f0                       ; 0xc22db vgabios.c:1463
    shr byte [bp-002h], 1                     ; d0 6e fe                    ; 0xc22dd vgabios.c:1466
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2                     ; 0xc22e0 vgabios.c:1467
    jmp short 0228ch                          ; eb a8                       ; 0xc22e2
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc22e4 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc22e7
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc22e9
    inc bx                                    ; 43                          ; 0xc22ec vgabios.c:1469
    jmp short 0226fh                          ; eb 80                       ; 0xc22ed vgabios.c:1470
    leave                                     ; c9                          ; 0xc22ef vgabios.c:1473
    pop di                                    ; 5f                          ; 0xc22f0
    pop si                                    ; 5e                          ; 0xc22f1
    retn 00004h                               ; c2 04 00                    ; 0xc22f2
  ; disGetNextSymbol 0xc22f5 LB 0x1f8e -> off=0x0 cb=000000000000009b uValue=00000000000c22f5 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc22f5 LB 0x9b
    push si                                   ; 56                          ; 0xc22f5 vgabios.c:1476
    push di                                   ; 57                          ; 0xc22f6
    enter 00008h, 000h                        ; c8 08 00 00                 ; 0xc22f7
    mov bh, al                                ; 88 c7                       ; 0xc22fb
    mov ch, dl                                ; 88 d5                       ; 0xc22fd
    mov al, cl                                ; 88 c8                       ; 0xc22ff
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc2301 vgabios.c:1483
    xor ah, ah                                ; 30 e4                       ; 0xc2304 vgabios.c:1484
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc2306
    xor dh, dh                                ; 30 f6                       ; 0xc2309
    imul dx                                   ; f7 ea                       ; 0xc230b
    mov dx, ax                                ; 89 c2                       ; 0xc230d
    sal dx, 006h                              ; c1 e2 06                    ; 0xc230f
    mov al, bl                                ; 88 d8                       ; 0xc2312
    xor ah, ah                                ; 30 e4                       ; 0xc2314
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2316
    add ax, dx                                ; 01 d0                       ; 0xc2319
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc231b
    mov al, bh                                ; 88 f8                       ; 0xc231e vgabios.c:1485
    xor ah, ah                                ; 30 e4                       ; 0xc2320
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2322
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc2325
    xor bl, bl                                ; 30 db                       ; 0xc2328 vgabios.c:1486
    jmp short 0236eh                          ; eb 42                       ; 0xc232a
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc232c vgabios.c:1490
    jnc short 02367h                          ; 73 37                       ; 0xc232e
    xor bh, bh                                ; 30 ff                       ; 0xc2330 vgabios.c:1492
    mov dl, bl                                ; 88 da                       ; 0xc2332 vgabios.c:1493
    xor dh, dh                                ; 30 f6                       ; 0xc2334
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc2336
    mov si, di                                ; 89 fe                       ; 0xc2339
    add si, dx                                ; 01 d6                       ; 0xc233b
    mov dl, byte [si]                         ; 8a 14                       ; 0xc233d
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc233f
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc2342
    mov dl, ah                                ; 88 e2                       ; 0xc2345
    xor dh, dh                                ; 30 f6                       ; 0xc2347
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc2349
    je short 02350h                           ; 74 02                       ; 0xc234c
    mov bh, ch                                ; 88 ef                       ; 0xc234e vgabios.c:1495
    mov dl, al                                ; 88 c2                       ; 0xc2350 vgabios.c:1497
    xor dh, dh                                ; 30 f6                       ; 0xc2352
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc2354
    add si, dx                                ; 01 d6                       ; 0xc2357
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc2359 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc235c
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc235e
    shr ah, 1                                 ; d0 ec                       ; 0xc2361 vgabios.c:1498
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2363 vgabios.c:1499
    jmp short 0232ch                          ; eb c5                       ; 0xc2365
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc2367 vgabios.c:1500
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2369
    jnc short 0238ah                          ; 73 1c                       ; 0xc236c
    mov al, bl                                ; 88 d8                       ; 0xc236e
    xor ah, ah                                ; 30 e4                       ; 0xc2370
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc2372
    xor dh, dh                                ; 30 f6                       ; 0xc2375
    imul dx                                   ; f7 ea                       ; 0xc2377
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2379
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc237c
    add dx, ax                                ; 01 c2                       ; 0xc237f
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc2381
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc2384
    xor al, al                                ; 30 c0                       ; 0xc2386
    jmp short 02330h                          ; eb a6                       ; 0xc2388
    leave                                     ; c9                          ; 0xc238a vgabios.c:1501
    pop di                                    ; 5f                          ; 0xc238b
    pop si                                    ; 5e                          ; 0xc238c
    retn 00002h                               ; c2 02 00                    ; 0xc238d
  ; disGetNextSymbol 0xc2390 LB 0x1ef3 -> off=0x0 cb=0000000000000188 uValue=00000000000c2390 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc2390 LB 0x188
    push bp                                   ; 55                          ; 0xc2390 vgabios.c:1504
    mov bp, sp                                ; 89 e5                       ; 0xc2391
    push si                                   ; 56                          ; 0xc2393
    push di                                   ; 57                          ; 0xc2394
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc2395
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2398
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc239b
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc239e
    mov si, cx                                ; 89 ce                       ; 0xc23a1
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc23a3 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23a6
    mov es, ax                                ; 8e c0                       ; 0xc23a9
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc23ab
    xor ah, ah                                ; 30 e4                       ; 0xc23ae vgabios.c:1512
    call 035d1h                               ; e8 1e 12                    ; 0xc23b0
    mov cl, al                                ; 88 c1                       ; 0xc23b3
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc23b5
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc23b8 vgabios.c:1513
    jne short 023bfh                          ; 75 03                       ; 0xc23ba
    jmp near 02511h                           ; e9 52 01                    ; 0xc23bc
    mov al, dl                                ; 88 d0                       ; 0xc23bf vgabios.c:1516
    xor ah, ah                                ; 30 e4                       ; 0xc23c1
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc23c3
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc23c6
    call 00a0bh                               ; e8 3f e6                    ; 0xc23c9
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc23cc vgabios.c:1517
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc23cf
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc23d2
    xor al, al                                ; 30 c0                       ; 0xc23d5
    shr ax, 008h                              ; c1 e8 08                    ; 0xc23d7
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc23da
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc23dd
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc23e0
    mov bx, 00084h                            ; bb 84 00                    ; 0xc23e3 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23e6
    mov es, ax                                ; 8e c0                       ; 0xc23e9
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc23eb
    xor ah, ah                                ; 30 e4                       ; 0xc23ee vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc23f0
    inc dx                                    ; 42                          ; 0xc23f2
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc23f3 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc23f6
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc23f9
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc23fc vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc23ff vgabios.c:1523
    xor bh, bh                                ; 30 ff                       ; 0xc2401
    mov di, bx                                ; 89 df                       ; 0xc2403
    sal di, 003h                              ; c1 e7 03                    ; 0xc2405
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc2408
    jne short 02459h                          ; 75 4a                       ; 0xc240d
    mul dx                                    ; f7 e2                       ; 0xc240f vgabios.c:1526
    add ax, ax                                ; 01 c0                       ; 0xc2411
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2413
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2415
    xor dh, dh                                ; 30 f6                       ; 0xc2418
    inc ax                                    ; 40                          ; 0xc241a
    mul dx                                    ; f7 e2                       ; 0xc241b
    mov bx, ax                                ; 89 c3                       ; 0xc241d
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc241f
    xor ah, ah                                ; 30 e4                       ; 0xc2422
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc2424
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2427
    xor dh, dh                                ; 30 f6                       ; 0xc242a
    add ax, dx                                ; 01 d0                       ; 0xc242c
    add ax, ax                                ; 01 c0                       ; 0xc242e
    mov dx, bx                                ; 89 da                       ; 0xc2430
    add dx, ax                                ; 01 c2                       ; 0xc2432
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2434 vgabios.c:1528
    xor ah, ah                                ; 30 e4                       ; 0xc2437
    mov bx, ax                                ; 89 c3                       ; 0xc2439
    sal bx, 008h                              ; c1 e3 08                    ; 0xc243b
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc243e
    add bx, ax                                ; 01 c3                       ; 0xc2441
    mov word [bp-020h], bx                    ; 89 5e e0                    ; 0xc2443
    mov ax, word [bp-020h]                    ; 8b 46 e0                    ; 0xc2446 vgabios.c:1529
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2449
    mov cx, si                                ; 89 f1                       ; 0xc244d
    mov di, dx                                ; 89 d7                       ; 0xc244f
    cld                                       ; fc                          ; 0xc2451
    jcxz 02456h                               ; e3 02                       ; 0xc2452
    rep stosw                                 ; f3 ab                       ; 0xc2454
    jmp near 02511h                           ; e9 b8 00                    ; 0xc2456 vgabios.c:1531
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc2459 vgabios.c:1534
    sal bx, 006h                              ; c1 e3 06                    ; 0xc245d
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc2460
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2464
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc2467 vgabios.c:1535
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc246b
    dec si                                    ; 4e                          ; 0xc246e vgabios.c:1536
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc246f
    je short 024c4h                           ; 74 50                       ; 0xc2472
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc2474 vgabios.c:1538
    xor bh, bh                                ; 30 ff                       ; 0xc2477
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2479
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc247c
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2480
    jc short 02494h                           ; 72 0f                       ; 0xc2483
    jbe short 0249bh                          ; 76 14                       ; 0xc2485
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2487
    je short 024f0h                           ; 74 64                       ; 0xc248a
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc248c
    je short 0249fh                           ; 74 0e                       ; 0xc248f
    jmp near 0250bh                           ; e9 77 00                    ; 0xc2491
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2494
    je short 024c6h                           ; 74 2d                       ; 0xc2497
    jmp short 0250bh                          ; eb 70                       ; 0xc2499
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc249b vgabios.c:1541
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc249f vgabios.c:1543
    xor ah, ah                                ; 30 e4                       ; 0xc24a2
    push ax                                   ; 50                          ; 0xc24a4
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc24a5
    push ax                                   ; 50                          ; 0xc24a8
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24a9
    push ax                                   ; 50                          ; 0xc24ac
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc24ad
    xor ch, ch                                ; 30 ed                       ; 0xc24b0
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24b2
    xor bh, bh                                ; 30 ff                       ; 0xc24b5
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc24b7
    xor dh, dh                                ; 30 f6                       ; 0xc24ba
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc24bc
    call 020d2h                               ; e8 10 fc                    ; 0xc24bf
    jmp short 0250bh                          ; eb 47                       ; 0xc24c2 vgabios.c:1544
    jmp short 02511h                          ; eb 4b                       ; 0xc24c4
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc24c6 vgabios.c:1546
    xor ah, ah                                ; 30 e4                       ; 0xc24c9
    push ax                                   ; 50                          ; 0xc24cb
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24cc
    push ax                                   ; 50                          ; 0xc24cf
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc24d0
    xor ch, ch                                ; 30 ed                       ; 0xc24d3
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24d5
    xor bh, bh                                ; 30 ff                       ; 0xc24d8
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc24da
    xor dh, dh                                ; 30 f6                       ; 0xc24dd
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc24df
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc24e2
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc24e5
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc24e8
    call 021e3h                               ; e8 f5 fc                    ; 0xc24eb
    jmp short 0250bh                          ; eb 1b                       ; 0xc24ee vgabios.c:1547
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24f0 vgabios.c:1549
    xor ah, ah                                ; 30 e4                       ; 0xc24f3
    push ax                                   ; 50                          ; 0xc24f5
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc24f6
    xor ch, ch                                ; 30 ed                       ; 0xc24f9
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24fb
    xor bh, bh                                ; 30 ff                       ; 0xc24fe
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2500
    xor dh, dh                                ; 30 f6                       ; 0xc2503
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2505
    call 022f5h                               ; e8 ea fd                    ; 0xc2508
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc250b vgabios.c:1556
    jmp near 0246eh                           ; e9 5d ff                    ; 0xc250e vgabios.c:1557
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2511 vgabios.c:1559
    pop di                                    ; 5f                          ; 0xc2514
    pop si                                    ; 5e                          ; 0xc2515
    pop bp                                    ; 5d                          ; 0xc2516
    retn                                      ; c3                          ; 0xc2517
  ; disGetNextSymbol 0xc2518 LB 0x1d6b -> off=0x0 cb=0000000000000181 uValue=00000000000c2518 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2518 LB 0x181
    push bp                                   ; 55                          ; 0xc2518 vgabios.c:1562
    mov bp, sp                                ; 89 e5                       ; 0xc2519
    push si                                   ; 56                          ; 0xc251b
    push di                                   ; 57                          ; 0xc251c
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc251d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2520
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2523
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc2526
    mov si, cx                                ; 89 ce                       ; 0xc2529
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc252b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc252e
    mov es, ax                                ; 8e c0                       ; 0xc2531
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2533
    xor ah, ah                                ; 30 e4                       ; 0xc2536 vgabios.c:1570
    call 035d1h                               ; e8 96 10                    ; 0xc2538
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc253b
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc253e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2541 vgabios.c:1571
    jne short 02548h                          ; 75 03                       ; 0xc2543
    jmp near 02692h                           ; e9 4a 01                    ; 0xc2545
    mov al, dl                                ; 88 d0                       ; 0xc2548 vgabios.c:1574
    xor ah, ah                                ; 30 e4                       ; 0xc254a
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc254c
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc254f
    call 00a0bh                               ; e8 b6 e4                    ; 0xc2552
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc2555 vgabios.c:1575
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2558
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc255b
    xor al, al                                ; 30 c0                       ; 0xc255e
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2560
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2563
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2566
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2569
    mov bx, 00084h                            ; bb 84 00                    ; 0xc256c vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc256f
    mov es, ax                                ; 8e c0                       ; 0xc2572
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2574
    xor ah, ah                                ; 30 e4                       ; 0xc2577 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc2579
    inc dx                                    ; 42                          ; 0xc257b
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc257c vgabios.c:47
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc257f
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc2582 vgabios.c:48
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2585 vgabios.c:1581
    mov bx, ax                                ; 89 c3                       ; 0xc2588
    sal bx, 003h                              ; c1 e3 03                    ; 0xc258a
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc258d
    jne short 025d6h                          ; 75 42                       ; 0xc2592
    mov ax, cx                                ; 89 c8                       ; 0xc2594 vgabios.c:1584
    mul dx                                    ; f7 e2                       ; 0xc2596
    add ax, ax                                ; 01 c0                       ; 0xc2598
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc259a
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc259c
    xor dh, dh                                ; 30 f6                       ; 0xc259f
    inc ax                                    ; 40                          ; 0xc25a1
    mul dx                                    ; f7 e2                       ; 0xc25a2
    mov bx, ax                                ; 89 c3                       ; 0xc25a4
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc25a6
    xor ah, ah                                ; 30 e4                       ; 0xc25a9
    mul cx                                    ; f7 e1                       ; 0xc25ab
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc25ad
    xor dh, dh                                ; 30 f6                       ; 0xc25b0
    add ax, dx                                ; 01 d0                       ; 0xc25b2
    add ax, ax                                ; 01 c0                       ; 0xc25b4
    add bx, ax                                ; 01 c3                       ; 0xc25b6
    dec si                                    ; 4e                          ; 0xc25b8 vgabios.c:1586
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25b9
    je short 02545h                           ; 74 87                       ; 0xc25bc
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc25be vgabios.c:1587
    xor ah, ah                                ; 30 e4                       ; 0xc25c1
    mov di, ax                                ; 89 c7                       ; 0xc25c3
    sal di, 003h                              ; c1 e7 03                    ; 0xc25c5
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc25c8 vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc25cc vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc25cf
    inc bx                                    ; 43                          ; 0xc25d2 vgabios.c:1588
    inc bx                                    ; 43                          ; 0xc25d3
    jmp short 025b8h                          ; eb e2                       ; 0xc25d4 vgabios.c:1589
    mov di, ax                                ; 89 c7                       ; 0xc25d6 vgabios.c:1594
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc25d8
    mov di, ax                                ; 89 c7                       ; 0xc25dc
    sal di, 006h                              ; c1 e7 06                    ; 0xc25de
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc25e1
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc25e5
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc25e8 vgabios.c:1595
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc25ec
    dec si                                    ; 4e                          ; 0xc25ef vgabios.c:1596
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25f0
    je short 02645h                           ; 74 50                       ; 0xc25f3
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc25f5 vgabios.c:1598
    xor bh, bh                                ; 30 ff                       ; 0xc25f8
    sal bx, 003h                              ; c1 e3 03                    ; 0xc25fa
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc25fd
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2601
    jc short 02615h                           ; 72 0f                       ; 0xc2604
    jbe short 0261ch                          ; 76 14                       ; 0xc2606
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2608
    je short 02671h                           ; 74 64                       ; 0xc260b
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc260d
    je short 02620h                           ; 74 0e                       ; 0xc2610
    jmp near 0268ch                           ; e9 77 00                    ; 0xc2612
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2615
    je short 02647h                           ; 74 2d                       ; 0xc2618
    jmp short 0268ch                          ; eb 70                       ; 0xc261a
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc261c vgabios.c:1601
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2620 vgabios.c:1603
    xor ah, ah                                ; 30 e4                       ; 0xc2623
    push ax                                   ; 50                          ; 0xc2625
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc2626
    push ax                                   ; 50                          ; 0xc2629
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc262a
    push ax                                   ; 50                          ; 0xc262d
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc262e
    xor ch, ch                                ; 30 ed                       ; 0xc2631
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2633
    xor bh, bh                                ; 30 ff                       ; 0xc2636
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2638
    xor dh, dh                                ; 30 f6                       ; 0xc263b
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc263d
    call 020d2h                               ; e8 8f fa                    ; 0xc2640
    jmp short 0268ch                          ; eb 47                       ; 0xc2643 vgabios.c:1604
    jmp short 02692h                          ; eb 4b                       ; 0xc2645
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2647 vgabios.c:1606
    xor ah, ah                                ; 30 e4                       ; 0xc264a
    push ax                                   ; 50                          ; 0xc264c
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc264d
    push ax                                   ; 50                          ; 0xc2650
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2651
    xor ch, ch                                ; 30 ed                       ; 0xc2654
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2656
    xor bh, bh                                ; 30 ff                       ; 0xc2659
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc265b
    xor dh, dh                                ; 30 f6                       ; 0xc265e
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2660
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc2663
    mov byte [bp-019h], ah                    ; 88 66 e7                    ; 0xc2666
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2669
    call 021e3h                               ; e8 74 fb                    ; 0xc266c
    jmp short 0268ch                          ; eb 1b                       ; 0xc266f vgabios.c:1607
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2671 vgabios.c:1609
    xor ah, ah                                ; 30 e4                       ; 0xc2674
    push ax                                   ; 50                          ; 0xc2676
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2677
    xor ch, ch                                ; 30 ed                       ; 0xc267a
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc267c
    xor bh, bh                                ; 30 ff                       ; 0xc267f
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2681
    xor dh, dh                                ; 30 f6                       ; 0xc2684
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2686
    call 022f5h                               ; e8 69 fc                    ; 0xc2689
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc268c vgabios.c:1616
    jmp near 025efh                           ; e9 5d ff                    ; 0xc268f vgabios.c:1617
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2692 vgabios.c:1619
    pop di                                    ; 5f                          ; 0xc2695
    pop si                                    ; 5e                          ; 0xc2696
    pop bp                                    ; 5d                          ; 0xc2697
    retn                                      ; c3                          ; 0xc2698
  ; disGetNextSymbol 0xc2699 LB 0x1bea -> off=0x0 cb=0000000000000173 uValue=00000000000c2699 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc2699 LB 0x173
    push bp                                   ; 55                          ; 0xc2699 vgabios.c:1622
    mov bp, sp                                ; 89 e5                       ; 0xc269a
    push si                                   ; 56                          ; 0xc269c
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc269d
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc26a0
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc26a3
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc26a6
    mov dx, cx                                ; 89 ca                       ; 0xc26a9
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc26ab vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26ae
    mov es, ax                                ; 8e c0                       ; 0xc26b1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc26b3
    xor ah, ah                                ; 30 e4                       ; 0xc26b6 vgabios.c:1629
    call 035d1h                               ; e8 16 0f                    ; 0xc26b8
    mov cl, al                                ; 88 c1                       ; 0xc26bb
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc26bd vgabios.c:1630
    je short 026e7h                           ; 74 26                       ; 0xc26bf
    mov bl, al                                ; 88 c3                       ; 0xc26c1 vgabios.c:1631
    xor bh, bh                                ; 30 ff                       ; 0xc26c3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc26c5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc26c8
    je short 026e7h                           ; 74 18                       ; 0xc26cd
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc26cf vgabios.c:1633
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc26d3
    jc short 026e3h                           ; 72 0c                       ; 0xc26d5
    jbe short 026edh                          ; 76 14                       ; 0xc26d7
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc26d9
    je short 026eah                           ; 74 0d                       ; 0xc26db
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc26dd
    je short 026edh                           ; 74 0c                       ; 0xc26df
    jmp short 026e7h                          ; eb 04                       ; 0xc26e1
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc26e3
    je short 0275eh                           ; 74 77                       ; 0xc26e5
    jmp near 02806h                           ; e9 1c 01                    ; 0xc26e7
    jmp near 027e4h                           ; e9 f7 00                    ; 0xc26ea
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc26ed vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26f0
    mov es, ax                                ; 8e c0                       ; 0xc26f3
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc26f5
    mov ax, dx                                ; 89 d0                       ; 0xc26f8 vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc26fa
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc26fc
    shr bx, 003h                              ; c1 eb 03                    ; 0xc26ff
    add bx, ax                                ; 01 c3                       ; 0xc2702
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2704 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2707
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc270a vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc270d
    mul dx                                    ; f7 e2                       ; 0xc270f
    add bx, ax                                ; 01 c3                       ; 0xc2711
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc2713 vgabios.c:1639
    and cl, 007h                              ; 80 e1 07                    ; 0xc2716
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2719
    sar ax, CL                                ; d3 f8                       ; 0xc271c
    xor ah, ah                                ; 30 e4                       ; 0xc271e vgabios.c:1640
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2720
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2723
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2725
    out DX, ax                                ; ef                          ; 0xc2728
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2729 vgabios.c:1641
    out DX, ax                                ; ef                          ; 0xc272c
    mov dx, bx                                ; 89 da                       ; 0xc272d vgabios.c:1642
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc272f
    call 035f9h                               ; e8 c4 0e                    ; 0xc2732
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2735 vgabios.c:1643
    je short 02742h                           ; 74 07                       ; 0xc2739
    mov ax, 01803h                            ; b8 03 18                    ; 0xc273b vgabios.c:1645
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc273e
    out DX, ax                                ; ef                          ; 0xc2741
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2742 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc2745
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2747
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc274a
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc274d vgabios.c:1648
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2750
    out DX, ax                                ; ef                          ; 0xc2753
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2754 vgabios.c:1649
    out DX, ax                                ; ef                          ; 0xc2757
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2758 vgabios.c:1650
    out DX, ax                                ; ef                          ; 0xc275b
    jmp short 026e7h                          ; eb 89                       ; 0xc275c vgabios.c:1651
    mov ax, dx                                ; 89 d0                       ; 0xc275e vgabios.c:1653
    shr ax, 1                                 ; d1 e8                       ; 0xc2760
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc2762
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc2765
    jne short 02774h                          ; 75 08                       ; 0xc276a
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc276c vgabios.c:1655
    shr bx, 002h                              ; c1 eb 02                    ; 0xc276f
    jmp short 0277ah                          ; eb 06                       ; 0xc2772 vgabios.c:1657
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2774 vgabios.c:1659
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2777
    add bx, ax                                ; 01 c3                       ; 0xc277a
    test dl, 001h                             ; f6 c2 01                    ; 0xc277c vgabios.c:1661
    je short 02784h                           ; 74 03                       ; 0xc277f
    add bh, 020h                              ; 80 c7 20                    ; 0xc2781
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc2784 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc2787
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2789
    mov al, cl                                ; 88 c8                       ; 0xc278c vgabios.c:1663
    xor ah, ah                                ; 30 e4                       ; 0xc278e
    mov si, ax                                ; 89 c6                       ; 0xc2790
    sal si, 003h                              ; c1 e6 03                    ; 0xc2792
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc2795
    jne short 027b5h                          ; 75 19                       ; 0xc279a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc279c vgabios.c:1665
    and AL, strict byte 003h                  ; 24 03                       ; 0xc279f
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc27a1
    sub ah, al                                ; 28 c4                       ; 0xc27a3
    mov cl, ah                                ; 88 e1                       ; 0xc27a5
    add cl, ah                                ; 00 e1                       ; 0xc27a7
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc27a9
    and dh, 003h                              ; 80 e6 03                    ; 0xc27ac
    sal dh, CL                                ; d2 e6                       ; 0xc27af
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc27b1 vgabios.c:1666
    jmp short 027c8h                          ; eb 13                       ; 0xc27b3 vgabios.c:1668
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc27b5 vgabios.c:1670
    and AL, strict byte 007h                  ; 24 07                       ; 0xc27b8
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc27ba
    sub cl, al                                ; 28 c1                       ; 0xc27bc
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc27be
    and dh, 001h                              ; 80 e6 01                    ; 0xc27c1
    sal dh, CL                                ; d2 e6                       ; 0xc27c4
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc27c6 vgabios.c:1671
    sal al, CL                                ; d2 e0                       ; 0xc27c8
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc27ca vgabios.c:1673
    je short 027d4h                           ; 74 04                       ; 0xc27ce
    xor dl, dh                                ; 30 f2                       ; 0xc27d0 vgabios.c:1675
    jmp short 027dah                          ; eb 06                       ; 0xc27d2 vgabios.c:1677
    not al                                    ; f6 d0                       ; 0xc27d4 vgabios.c:1679
    and dl, al                                ; 20 c2                       ; 0xc27d6
    or dl, dh                                 ; 08 f2                       ; 0xc27d8 vgabios.c:1680
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc27da vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc27dd
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc27df
    jmp short 02806h                          ; eb 22                       ; 0xc27e2 vgabios.c:1683
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc27e4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc27e7
    mov es, ax                                ; 8e c0                       ; 0xc27ea
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc27ec
    sal bx, 003h                              ; c1 e3 03                    ; 0xc27ef vgabios.c:48
    mov ax, dx                                ; 89 d0                       ; 0xc27f2
    mul bx                                    ; f7 e3                       ; 0xc27f4
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27f6
    add bx, ax                                ; 01 c3                       ; 0xc27f9
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc27fb vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc27fe
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2800
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2803
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2806 vgabios.c:1693
    pop si                                    ; 5e                          ; 0xc2809
    pop bp                                    ; 5d                          ; 0xc280a
    retn                                      ; c3                          ; 0xc280b
  ; disGetNextSymbol 0xc280c LB 0x1a77 -> off=0x0 cb=0000000000000258 uValue=00000000000c280c 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc280c LB 0x258
    push bp                                   ; 55                          ; 0xc280c vgabios.c:1696
    mov bp, sp                                ; 89 e5                       ; 0xc280d
    push si                                   ; 56                          ; 0xc280f
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc2810
    mov ch, al                                ; 88 c5                       ; 0xc2813
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc2815
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2818
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc281b vgabios.c:1704
    jne short 0282eh                          ; 75 0e                       ; 0xc281e
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc2820 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2823
    mov es, ax                                ; 8e c0                       ; 0xc2826
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2828
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc282b vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc282e vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2831
    mov es, ax                                ; 8e c0                       ; 0xc2834
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2836
    xor ah, ah                                ; 30 e4                       ; 0xc2839 vgabios.c:1709
    call 035d1h                               ; e8 93 0d                    ; 0xc283b
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc283e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2841 vgabios.c:1710
    je short 028abh                           ; 74 66                       ; 0xc2843
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2845 vgabios.c:1713
    xor ah, ah                                ; 30 e4                       ; 0xc2848
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc284a
    lea dx, [bp-016h]                         ; 8d 56 ea                    ; 0xc284d
    call 00a0bh                               ; e8 b8 e1                    ; 0xc2850
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc2853 vgabios.c:1714
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2856
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc2859
    xor al, al                                ; 30 c0                       ; 0xc285c
    shr ax, 008h                              ; c1 e8 08                    ; 0xc285e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2861
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2864 vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2867
    mov es, dx                                ; 8e c2                       ; 0xc286a
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc286c
    xor dh, dh                                ; 30 f6                       ; 0xc286f vgabios.c:38
    inc dx                                    ; 42                          ; 0xc2871
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc2872
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2875 vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2878
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc287b vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc287e vgabios.c:1720
    jc short 02891h                           ; 72 0e                       ; 0xc2881
    jbe short 02899h                          ; 76 14                       ; 0xc2883
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc2885
    je short 028aeh                           ; 74 24                       ; 0xc2888
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc288a
    je short 028a4h                           ; 74 15                       ; 0xc288d
    jmp short 028b5h                          ; eb 24                       ; 0xc288f
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2891
    jne short 028b5h                          ; 75 1f                       ; 0xc2894
    jmp near 029bbh                           ; e9 22 01                    ; 0xc2896
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00                 ; 0xc2899 vgabios.c:1727
    jbe short 028b2h                          ; 76 13                       ; 0xc289d
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc289f
    jmp short 028b2h                          ; eb 0e                       ; 0xc28a2 vgabios.c:1728
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc28a4 vgabios.c:1731
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc28a6
    jmp short 028b2h                          ; eb 07                       ; 0xc28a9 vgabios.c:1732
    jmp near 02a5eh                           ; e9 b0 01                    ; 0xc28ab
    mov byte [bp-004h], 000h                  ; c6 46 fc 00                 ; 0xc28ae vgabios.c:1735
    jmp near 029bbh                           ; e9 06 01                    ; 0xc28b2 vgabios.c:1736
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc28b5 vgabios.c:1740
    xor ah, ah                                ; 30 e4                       ; 0xc28b8
    mov bx, ax                                ; 89 c3                       ; 0xc28ba
    sal bx, 003h                              ; c1 e3 03                    ; 0xc28bc
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc28bf
    jne short 02908h                          ; 75 42                       ; 0xc28c4
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc28c6 vgabios.c:1743
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc28c9
    add ax, ax                                ; 01 c0                       ; 0xc28cc
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc28ce
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc28d0
    xor dh, dh                                ; 30 f6                       ; 0xc28d3
    inc ax                                    ; 40                          ; 0xc28d5
    mul dx                                    ; f7 e2                       ; 0xc28d6
    mov si, ax                                ; 89 c6                       ; 0xc28d8
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc28da
    xor ah, ah                                ; 30 e4                       ; 0xc28dd
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc28df
    mov dx, ax                                ; 89 c2                       ; 0xc28e2
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc28e4
    xor ah, ah                                ; 30 e4                       ; 0xc28e7
    add ax, dx                                ; 01 d0                       ; 0xc28e9
    add ax, ax                                ; 01 c0                       ; 0xc28eb
    add si, ax                                ; 01 c6                       ; 0xc28ed
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc28ef vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc28f3 vgabios.c:42
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc28f6 vgabios.c:1748
    jne short 02937h                          ; 75 3c                       ; 0xc28f9
    inc si                                    ; 46                          ; 0xc28fb vgabios.c:1749
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc28fc vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2900
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2903
    jmp short 02937h                          ; eb 2f                       ; 0xc2906 vgabios.c:1751
    mov si, ax                                ; 89 c6                       ; 0xc2908 vgabios.c:1754
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc290a
    mov si, ax                                ; 89 c6                       ; 0xc290e
    sal si, 006h                              ; c1 e6 06                    ; 0xc2910
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc2913
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2917 vgabios.c:1755
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc291b vgabios.c:1756
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc291f
    jc short 02932h                           ; 72 0e                       ; 0xc2922
    jbe short 02939h                          ; 76 13                       ; 0xc2924
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2926
    je short 02989h                           ; 74 5e                       ; 0xc2929
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc292b
    je short 0293dh                           ; 74 0d                       ; 0xc292e
    jmp short 029a8h                          ; eb 76                       ; 0xc2930
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2932
    je short 02967h                           ; 74 30                       ; 0xc2935
    jmp short 029a8h                          ; eb 6f                       ; 0xc2937
    or byte [bp-00ah], 001h                   ; 80 4e f6 01                 ; 0xc2939 vgabios.c:1759
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc293d vgabios.c:1761
    xor ah, ah                                ; 30 e4                       ; 0xc2940
    push ax                                   ; 50                          ; 0xc2942
    mov al, dl                                ; 88 d0                       ; 0xc2943
    push ax                                   ; 50                          ; 0xc2945
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2946
    push ax                                   ; 50                          ; 0xc2949
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc294a
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc294d
    xor bh, bh                                ; 30 ff                       ; 0xc2950
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2952
    xor dh, dh                                ; 30 f6                       ; 0xc2955
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc2957
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc295a
    mov cx, ax                                ; 89 c1                       ; 0xc295d
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc295f
    call 020d2h                               ; e8 6d f7                    ; 0xc2962
    jmp short 029a8h                          ; eb 41                       ; 0xc2965 vgabios.c:1762
    push ax                                   ; 50                          ; 0xc2967 vgabios.c:1764
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2968
    push ax                                   ; 50                          ; 0xc296b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc296c
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc296f
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2972
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2975
    xor bh, bh                                ; 30 ff                       ; 0xc2978
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc297a
    xor dh, dh                                ; 30 f6                       ; 0xc297d
    mov al, ch                                ; 88 e8                       ; 0xc297f
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc2981
    call 021e3h                               ; e8 5c f8                    ; 0xc2984
    jmp short 029a8h                          ; eb 1f                       ; 0xc2987 vgabios.c:1765
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2989 vgabios.c:1767
    push ax                                   ; 50                          ; 0xc298c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc298d
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2990
    xor bh, bh                                ; 30 ff                       ; 0xc2993
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2995
    xor dh, dh                                ; 30 f6                       ; 0xc2998
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc299a
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc299d
    mov cx, ax                                ; 89 c1                       ; 0xc29a0
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc29a2
    call 022f5h                               ; e8 4d f9                    ; 0xc29a5
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc29a8 vgabios.c:1775
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc29ab vgabios.c:1777
    xor ah, ah                                ; 30 e4                       ; 0xc29ae
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc29b0
    jne short 029bbh                          ; 75 06                       ; 0xc29b3
    mov byte [bp-004h], ah                    ; 88 66 fc                    ; 0xc29b5 vgabios.c:1778
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc29b8 vgabios.c:1779
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc29bb vgabios.c:1784
    xor ah, ah                                ; 30 e4                       ; 0xc29be
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc29c0
    jne short 02a26h                          ; 75 61                       ; 0xc29c3
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc29c5 vgabios.c:1786
    xor bh, bh                                ; 30 ff                       ; 0xc29c8
    sal bx, 003h                              ; c1 e3 03                    ; 0xc29ca
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc29cd
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc29d0
    mov cl, byte [bp-010h]                    ; 8a 4e f0                    ; 0xc29d2
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc29d5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc29d7
    jne short 02a28h                          ; 75 4a                       ; 0xc29dc
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29de vgabios.c:1788
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc29e1
    add ax, ax                                ; 01 c0                       ; 0xc29e4
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc29e6
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc29e8
    xor dh, dh                                ; 30 f6                       ; 0xc29eb
    inc ax                                    ; 40                          ; 0xc29ed
    mul dx                                    ; f7 e2                       ; 0xc29ee
    mov si, ax                                ; 89 c6                       ; 0xc29f0
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc29f2
    xor ah, ah                                ; 30 e4                       ; 0xc29f5
    dec ax                                    ; 48                          ; 0xc29f7
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc29f8
    mov dx, ax                                ; 89 c2                       ; 0xc29fb
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc29fd
    xor ah, ah                                ; 30 e4                       ; 0xc2a00
    add ax, dx                                ; 01 d0                       ; 0xc2a02
    add ax, ax                                ; 01 c0                       ; 0xc2a04
    add si, ax                                ; 01 c6                       ; 0xc2a06
    inc si                                    ; 46                          ; 0xc2a08 vgabios.c:1789
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2a09 vgabios.c:35
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2a0d
    push strict byte 00001h                   ; 6a 01                       ; 0xc2a10 vgabios.c:1790
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a12
    xor ah, ah                                ; 30 e4                       ; 0xc2a15
    push ax                                   ; 50                          ; 0xc2a17
    mov al, cl                                ; 88 c8                       ; 0xc2a18
    push ax                                   ; 50                          ; 0xc2a1a
    mov al, ch                                ; 88 e8                       ; 0xc2a1b
    push ax                                   ; 50                          ; 0xc2a1d
    xor dh, dh                                ; 30 f6                       ; 0xc2a1e
    xor cx, cx                                ; 31 c9                       ; 0xc2a20
    xor bx, bx                                ; 31 db                       ; 0xc2a22
    jmp short 02a3ah                          ; eb 14                       ; 0xc2a24 vgabios.c:1792
    jmp short 02a43h                          ; eb 1b                       ; 0xc2a26
    push strict byte 00001h                   ; 6a 01                       ; 0xc2a28 vgabios.c:1794
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a2a
    push ax                                   ; 50                          ; 0xc2a2d
    mov al, cl                                ; 88 c8                       ; 0xc2a2e
    push ax                                   ; 50                          ; 0xc2a30
    mov al, ch                                ; 88 e8                       ; 0xc2a31
    push ax                                   ; 50                          ; 0xc2a33
    xor cx, cx                                ; 31 c9                       ; 0xc2a34
    xor bx, bx                                ; 31 db                       ; 0xc2a36
    xor dx, dx                                ; 31 d2                       ; 0xc2a38
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a3a
    call 01a42h                               ; e8 02 f0                    ; 0xc2a3d
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2a40 vgabios.c:1796
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2a43 vgabios.c:1800
    xor ah, ah                                ; 30 e4                       ; 0xc2a46
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc2a48
    sal word [bp-014h], 008h                  ; c1 66 ec 08                 ; 0xc2a4b
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2a4f
    add word [bp-014h], ax                    ; 01 46 ec                    ; 0xc2a52
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc2a55 vgabios.c:1801
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a58
    call 0120eh                               ; e8 b0 e7                    ; 0xc2a5b
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a5e vgabios.c:1802
    pop si                                    ; 5e                          ; 0xc2a61
    pop bp                                    ; 5d                          ; 0xc2a62
    retn                                      ; c3                          ; 0xc2a63
  ; disGetNextSymbol 0xc2a64 LB 0x181f -> off=0x0 cb=000000000000002c uValue=00000000000c2a64 'get_font_access'
get_font_access:                             ; 0xc2a64 LB 0x2c
    push bp                                   ; 55                          ; 0xc2a64 vgabios.c:1805
    mov bp, sp                                ; 89 e5                       ; 0xc2a65
    push dx                                   ; 52                          ; 0xc2a67
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a68 vgabios.c:1807
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a6b
    out DX, ax                                ; ef                          ; 0xc2a6e
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2a6f vgabios.c:1808
    out DX, ax                                ; ef                          ; 0xc2a72
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2a73 vgabios.c:1809
    out DX, ax                                ; ef                          ; 0xc2a76
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2a77 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc2a7a
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2a7b vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2a7e
    out DX, ax                                ; ef                          ; 0xc2a81
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2a82 vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2a85
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2a86 vgabios.c:1813
    out DX, ax                                ; ef                          ; 0xc2a89
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a8a vgabios.c:1814
    pop dx                                    ; 5a                          ; 0xc2a8d
    pop bp                                    ; 5d                          ; 0xc2a8e
    retn                                      ; c3                          ; 0xc2a8f
  ; disGetNextSymbol 0xc2a90 LB 0x17f3 -> off=0x0 cb=000000000000003c uValue=00000000000c2a90 'release_font_access'
release_font_access:                         ; 0xc2a90 LB 0x3c
    push bp                                   ; 55                          ; 0xc2a90 vgabios.c:1816
    mov bp, sp                                ; 89 e5                       ; 0xc2a91
    push dx                                   ; 52                          ; 0xc2a93
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a94 vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a97
    out DX, ax                                ; ef                          ; 0xc2a9a
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2a9b vgabios.c:1819
    out DX, ax                                ; ef                          ; 0xc2a9e
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2a9f vgabios.c:1820
    out DX, ax                                ; ef                          ; 0xc2aa2
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2aa3 vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2aa6
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2aa7 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc2aaa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2aab
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2aad
    sal ax, 002h                              ; c1 e0 02                    ; 0xc2ab0
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc2ab3
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2ab5
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2ab8
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2aba
    out DX, ax                                ; ef                          ; 0xc2abd
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2abe vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc2ac1
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2ac2 vgabios.c:1824
    out DX, ax                                ; ef                          ; 0xc2ac5
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2ac6 vgabios.c:1825
    pop dx                                    ; 5a                          ; 0xc2ac9
    pop bp                                    ; 5d                          ; 0xc2aca
    retn                                      ; c3                          ; 0xc2acb
  ; disGetNextSymbol 0xc2acc LB 0x17b7 -> off=0x0 cb=00000000000000b1 uValue=00000000000c2acc 'set_scan_lines'
set_scan_lines:                              ; 0xc2acc LB 0xb1
    push bp                                   ; 55                          ; 0xc2acc vgabios.c:1827
    mov bp, sp                                ; 89 e5                       ; 0xc2acd
    push bx                                   ; 53                          ; 0xc2acf
    push cx                                   ; 51                          ; 0xc2ad0
    push dx                                   ; 52                          ; 0xc2ad1
    push si                                   ; 56                          ; 0xc2ad2
    push di                                   ; 57                          ; 0xc2ad3
    mov bl, al                                ; 88 c3                       ; 0xc2ad4
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2ad6 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ad9
    mov es, ax                                ; 8e c0                       ; 0xc2adc
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2ade
    mov cx, si                                ; 89 f1                       ; 0xc2ae1 vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2ae3 vgabios.c:1833
    mov dx, si                                ; 89 f2                       ; 0xc2ae5
    out DX, AL                                ; ee                          ; 0xc2ae7
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2ae8 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc2aeb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2aec
    mov ah, al                                ; 88 c4                       ; 0xc2aee vgabios.c:1835
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2af0
    mov al, bl                                ; 88 d8                       ; 0xc2af3
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2af5
    or al, ah                                 ; 08 e0                       ; 0xc2af7
    out DX, AL                                ; ee                          ; 0xc2af9 vgabios.c:1836
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2afa vgabios.c:1837
    jne short 02b07h                          ; 75 08                       ; 0xc2afd
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2aff vgabios.c:1839
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2b02
    jmp short 02b14h                          ; eb 0d                       ; 0xc2b05 vgabios.c:1841
    mov dl, bl                                ; 88 da                       ; 0xc2b07 vgabios.c:1843
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2b09
    xor dh, dh                                ; 30 f6                       ; 0xc2b0c
    mov al, bl                                ; 88 d8                       ; 0xc2b0e
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2b10
    xor ah, ah                                ; 30 e4                       ; 0xc2b12
    call 01107h                               ; e8 f0 e5                    ; 0xc2b14
    xor bh, bh                                ; 30 ff                       ; 0xc2b17 vgabios.c:1845
    mov si, 00085h                            ; be 85 00                    ; 0xc2b19 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b1c
    mov es, ax                                ; 8e c0                       ; 0xc2b1f
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2b21
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2b24 vgabios.c:1846
    mov dx, cx                                ; 89 ca                       ; 0xc2b26
    out DX, AL                                ; ee                          ; 0xc2b28
    mov si, cx                                ; 89 ce                       ; 0xc2b29 vgabios.c:1847
    inc si                                    ; 46                          ; 0xc2b2b
    mov dx, si                                ; 89 f2                       ; 0xc2b2c
    in AL, DX                                 ; ec                          ; 0xc2b2e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b2f
    mov di, ax                                ; 89 c7                       ; 0xc2b31
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2b33 vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2b35
    out DX, AL                                ; ee                          ; 0xc2b37
    mov dx, si                                ; 89 f2                       ; 0xc2b38 vgabios.c:1849
    in AL, DX                                 ; ec                          ; 0xc2b3a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b3b
    mov dl, al                                ; 88 c2                       ; 0xc2b3d vgabios.c:1850
    and dl, 002h                              ; 80 e2 02                    ; 0xc2b3f
    xor dh, dh                                ; 30 f6                       ; 0xc2b42
    sal dx, 007h                              ; c1 e2 07                    ; 0xc2b44
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2b47
    xor ah, ah                                ; 30 e4                       ; 0xc2b49
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2b4b
    add ax, dx                                ; 01 d0                       ; 0xc2b4e
    inc ax                                    ; 40                          ; 0xc2b50
    add ax, di                                ; 01 f8                       ; 0xc2b51
    xor dx, dx                                ; 31 d2                       ; 0xc2b53 vgabios.c:1851
    div bx                                    ; f7 f3                       ; 0xc2b55
    mov dl, al                                ; 88 c2                       ; 0xc2b57 vgabios.c:1852
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2b59
    mov si, 00084h                            ; be 84 00                    ; 0xc2b5b vgabios.c:42
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2b5e
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2b61 vgabios.c:47
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2b64
    xor ah, ah                                ; 30 e4                       ; 0xc2b67 vgabios.c:1854
    mul dx                                    ; f7 e2                       ; 0xc2b69
    add ax, ax                                ; 01 c0                       ; 0xc2b6b
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2b6d vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2b70
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2b73 vgabios.c:1855
    pop di                                    ; 5f                          ; 0xc2b76
    pop si                                    ; 5e                          ; 0xc2b77
    pop dx                                    ; 5a                          ; 0xc2b78
    pop cx                                    ; 59                          ; 0xc2b79
    pop bx                                    ; 5b                          ; 0xc2b7a
    pop bp                                    ; 5d                          ; 0xc2b7b
    retn                                      ; c3                          ; 0xc2b7c
  ; disGetNextSymbol 0xc2b7d LB 0x1706 -> off=0x0 cb=0000000000000080 uValue=00000000000c2b7d 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2b7d LB 0x80
    push bp                                   ; 55                          ; 0xc2b7d vgabios.c:1857
    mov bp, sp                                ; 89 e5                       ; 0xc2b7e
    push si                                   ; 56                          ; 0xc2b80
    push di                                   ; 57                          ; 0xc2b81
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2b82
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2b85
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc2b88
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2b8b
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc2b8e
    call 02a64h                               ; e8 d0 fe                    ; 0xc2b91 vgabios.c:1862
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2b94 vgabios.c:1863
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2b97
    xor ah, ah                                ; 30 e4                       ; 0xc2b99
    mov bx, ax                                ; 89 c3                       ; 0xc2b9b
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2b9d
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2ba0
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2ba3
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2ba5
    add bx, ax                                ; 01 c3                       ; 0xc2ba8
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2baa
    xor bx, bx                                ; 31 db                       ; 0xc2bad vgabios.c:1864
    cmp bx, word [bp-00eh]                    ; 3b 5e f2                    ; 0xc2baf
    jnc short 02be3h                          ; 73 2f                       ; 0xc2bb2
    mov cl, byte [bp+008h]                    ; 8a 4e 08                    ; 0xc2bb4 vgabios.c:1866
    xor ch, ch                                ; 30 ed                       ; 0xc2bb7
    mov ax, bx                                ; 89 d8                       ; 0xc2bb9
    mul cx                                    ; f7 e1                       ; 0xc2bbb
    mov si, word [bp-00ah]                    ; 8b 76 f6                    ; 0xc2bbd
    add si, ax                                ; 01 c6                       ; 0xc2bc0
    mov ax, word [bp+004h]                    ; 8b 46 04                    ; 0xc2bc2 vgabios.c:1867
    add ax, bx                                ; 01 d8                       ; 0xc2bc5
    sal ax, 005h                              ; c1 e0 05                    ; 0xc2bc7
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc2bca
    add di, ax                                ; 01 c7                       ; 0xc2bcd
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc2bcf vgabios.c:1868
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2bd2
    mov es, ax                                ; 8e c0                       ; 0xc2bd5
    cld                                       ; fc                          ; 0xc2bd7
    jcxz 02be0h                               ; e3 06                       ; 0xc2bd8
    push DS                                   ; 1e                          ; 0xc2bda
    mov ds, dx                                ; 8e da                       ; 0xc2bdb
    rep movsb                                 ; f3 a4                       ; 0xc2bdd
    pop DS                                    ; 1f                          ; 0xc2bdf
    inc bx                                    ; 43                          ; 0xc2be0 vgabios.c:1869
    jmp short 02bafh                          ; eb cc                       ; 0xc2be1
    call 02a90h                               ; e8 aa fe                    ; 0xc2be3 vgabios.c:1870
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2be6 vgabios.c:1871
    jc short 02bf4h                           ; 72 08                       ; 0xc2bea
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2bec vgabios.c:1873
    xor ah, ah                                ; 30 e4                       ; 0xc2bef
    call 02acch                               ; e8 d8 fe                    ; 0xc2bf1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2bf4 vgabios.c:1875
    pop di                                    ; 5f                          ; 0xc2bf7
    pop si                                    ; 5e                          ; 0xc2bf8
    pop bp                                    ; 5d                          ; 0xc2bf9
    retn 00006h                               ; c2 06 00                    ; 0xc2bfa
  ; disGetNextSymbol 0xc2bfd LB 0x1686 -> off=0x0 cb=000000000000006e uValue=00000000000c2bfd 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2bfd LB 0x6e
    push bp                                   ; 55                          ; 0xc2bfd vgabios.c:1877
    mov bp, sp                                ; 89 e5                       ; 0xc2bfe
    push bx                                   ; 53                          ; 0xc2c00
    push cx                                   ; 51                          ; 0xc2c01
    push si                                   ; 56                          ; 0xc2c02
    push di                                   ; 57                          ; 0xc2c03
    push ax                                   ; 50                          ; 0xc2c04
    push ax                                   ; 50                          ; 0xc2c05
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c06
    call 02a64h                               ; e8 58 fe                    ; 0xc2c09 vgabios.c:1881
    mov al, dl                                ; 88 d0                       ; 0xc2c0c vgabios.c:1882
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c0e
    xor ah, ah                                ; 30 e4                       ; 0xc2c10
    mov bx, ax                                ; 89 c3                       ; 0xc2c12
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2c14
    mov al, dl                                ; 88 d0                       ; 0xc2c17
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c19
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c1b
    add bx, ax                                ; 01 c3                       ; 0xc2c1e
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c20
    xor bx, bx                                ; 31 db                       ; 0xc2c23 vgabios.c:1883
    jmp short 02c2dh                          ; eb 06                       ; 0xc2c25
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c27
    jnc short 02c53h                          ; 73 26                       ; 0xc2c2b
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2c2d vgabios.c:1885
    mov di, bx                                ; 89 df                       ; 0xc2c30 vgabios.c:1886
    sal di, 005h                              ; c1 e7 05                    ; 0xc2c32
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c35
    add si, 05d6ch                            ; 81 c6 6c 5d                 ; 0xc2c38 vgabios.c:1887
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2c3c
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2c3f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c42
    mov es, ax                                ; 8e c0                       ; 0xc2c45
    cld                                       ; fc                          ; 0xc2c47
    jcxz 02c50h                               ; e3 06                       ; 0xc2c48
    push DS                                   ; 1e                          ; 0xc2c4a
    mov ds, dx                                ; 8e da                       ; 0xc2c4b
    rep movsb                                 ; f3 a4                       ; 0xc2c4d
    pop DS                                    ; 1f                          ; 0xc2c4f
    inc bx                                    ; 43                          ; 0xc2c50 vgabios.c:1888
    jmp short 02c27h                          ; eb d4                       ; 0xc2c51
    call 02a90h                               ; e8 3a fe                    ; 0xc2c53 vgabios.c:1889
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2c56 vgabios.c:1890
    jc short 02c62h                           ; 72 06                       ; 0xc2c5a
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2c5c vgabios.c:1892
    call 02acch                               ; e8 6a fe                    ; 0xc2c5f
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2c62 vgabios.c:1894
    pop di                                    ; 5f                          ; 0xc2c65
    pop si                                    ; 5e                          ; 0xc2c66
    pop cx                                    ; 59                          ; 0xc2c67
    pop bx                                    ; 5b                          ; 0xc2c68
    pop bp                                    ; 5d                          ; 0xc2c69
    retn                                      ; c3                          ; 0xc2c6a
  ; disGetNextSymbol 0xc2c6b LB 0x1618 -> off=0x0 cb=0000000000000070 uValue=00000000000c2c6b 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2c6b LB 0x70
    push bp                                   ; 55                          ; 0xc2c6b vgabios.c:1896
    mov bp, sp                                ; 89 e5                       ; 0xc2c6c
    push bx                                   ; 53                          ; 0xc2c6e
    push cx                                   ; 51                          ; 0xc2c6f
    push si                                   ; 56                          ; 0xc2c70
    push di                                   ; 57                          ; 0xc2c71
    push ax                                   ; 50                          ; 0xc2c72
    push ax                                   ; 50                          ; 0xc2c73
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c74
    call 02a64h                               ; e8 ea fd                    ; 0xc2c77 vgabios.c:1900
    mov al, dl                                ; 88 d0                       ; 0xc2c7a vgabios.c:1901
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c7c
    xor ah, ah                                ; 30 e4                       ; 0xc2c7e
    mov bx, ax                                ; 89 c3                       ; 0xc2c80
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2c82
    mov al, dl                                ; 88 d0                       ; 0xc2c85
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c87
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c89
    add bx, ax                                ; 01 c3                       ; 0xc2c8c
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c8e
    xor bx, bx                                ; 31 db                       ; 0xc2c91 vgabios.c:1902
    jmp short 02c9bh                          ; eb 06                       ; 0xc2c93
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c95
    jnc short 02cc3h                          ; 73 28                       ; 0xc2c99
    mov si, bx                                ; 89 de                       ; 0xc2c9b vgabios.c:1904
    sal si, 003h                              ; c1 e6 03                    ; 0xc2c9d
    mov di, bx                                ; 89 df                       ; 0xc2ca0 vgabios.c:1905
    sal di, 005h                              ; c1 e7 05                    ; 0xc2ca2
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2ca5
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2ca8 vgabios.c:1906
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2cac
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2caf
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2cb2
    mov es, ax                                ; 8e c0                       ; 0xc2cb5
    cld                                       ; fc                          ; 0xc2cb7
    jcxz 02cc0h                               ; e3 06                       ; 0xc2cb8
    push DS                                   ; 1e                          ; 0xc2cba
    mov ds, dx                                ; 8e da                       ; 0xc2cbb
    rep movsb                                 ; f3 a4                       ; 0xc2cbd
    pop DS                                    ; 1f                          ; 0xc2cbf
    inc bx                                    ; 43                          ; 0xc2cc0 vgabios.c:1907
    jmp short 02c95h                          ; eb d2                       ; 0xc2cc1
    call 02a90h                               ; e8 ca fd                    ; 0xc2cc3 vgabios.c:1908
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2cc6 vgabios.c:1909
    jc short 02cd2h                           ; 72 06                       ; 0xc2cca
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2ccc vgabios.c:1911
    call 02acch                               ; e8 fa fd                    ; 0xc2ccf
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2cd2 vgabios.c:1913
    pop di                                    ; 5f                          ; 0xc2cd5
    pop si                                    ; 5e                          ; 0xc2cd6
    pop cx                                    ; 59                          ; 0xc2cd7
    pop bx                                    ; 5b                          ; 0xc2cd8
    pop bp                                    ; 5d                          ; 0xc2cd9
    retn                                      ; c3                          ; 0xc2cda
  ; disGetNextSymbol 0xc2cdb LB 0x15a8 -> off=0x0 cb=0000000000000070 uValue=00000000000c2cdb 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2cdb LB 0x70
    push bp                                   ; 55                          ; 0xc2cdb vgabios.c:1916
    mov bp, sp                                ; 89 e5                       ; 0xc2cdc
    push bx                                   ; 53                          ; 0xc2cde
    push cx                                   ; 51                          ; 0xc2cdf
    push si                                   ; 56                          ; 0xc2ce0
    push di                                   ; 57                          ; 0xc2ce1
    push ax                                   ; 50                          ; 0xc2ce2
    push ax                                   ; 50                          ; 0xc2ce3
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2ce4
    call 02a64h                               ; e8 7a fd                    ; 0xc2ce7 vgabios.c:1920
    mov al, dl                                ; 88 d0                       ; 0xc2cea vgabios.c:1921
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2cec
    xor ah, ah                                ; 30 e4                       ; 0xc2cee
    mov bx, ax                                ; 89 c3                       ; 0xc2cf0
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2cf2
    mov al, dl                                ; 88 d0                       ; 0xc2cf5
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2cf7
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2cf9
    add bx, ax                                ; 01 c3                       ; 0xc2cfc
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2cfe
    xor bx, bx                                ; 31 db                       ; 0xc2d01 vgabios.c:1922
    jmp short 02d0bh                          ; eb 06                       ; 0xc2d03
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2d05
    jnc short 02d33h                          ; 73 28                       ; 0xc2d09
    mov si, bx                                ; 89 de                       ; 0xc2d0b vgabios.c:1924
    sal si, 004h                              ; c1 e6 04                    ; 0xc2d0d
    mov di, bx                                ; 89 df                       ; 0xc2d10 vgabios.c:1925
    sal di, 005h                              ; c1 e7 05                    ; 0xc2d12
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d15
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2d18 vgabios.c:1926
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2d1c
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d1f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d22
    mov es, ax                                ; 8e c0                       ; 0xc2d25
    cld                                       ; fc                          ; 0xc2d27
    jcxz 02d30h                               ; e3 06                       ; 0xc2d28
    push DS                                   ; 1e                          ; 0xc2d2a
    mov ds, dx                                ; 8e da                       ; 0xc2d2b
    rep movsb                                 ; f3 a4                       ; 0xc2d2d
    pop DS                                    ; 1f                          ; 0xc2d2f
    inc bx                                    ; 43                          ; 0xc2d30 vgabios.c:1927
    jmp short 02d05h                          ; eb d2                       ; 0xc2d31
    call 02a90h                               ; e8 5a fd                    ; 0xc2d33 vgabios.c:1928
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2d36 vgabios.c:1929
    jc short 02d42h                           ; 72 06                       ; 0xc2d3a
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2d3c vgabios.c:1931
    call 02acch                               ; e8 8a fd                    ; 0xc2d3f
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2d42 vgabios.c:1933
    pop di                                    ; 5f                          ; 0xc2d45
    pop si                                    ; 5e                          ; 0xc2d46
    pop cx                                    ; 59                          ; 0xc2d47
    pop bx                                    ; 5b                          ; 0xc2d48
    pop bp                                    ; 5d                          ; 0xc2d49
    retn                                      ; c3                          ; 0xc2d4a
  ; disGetNextSymbol 0xc2d4b LB 0x1538 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d4b 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2d4b LB 0x5
    push bp                                   ; 55                          ; 0xc2d4b vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2d4c
    pop bp                                    ; 5d                          ; 0xc2d4e vgabios.c:1940
    retn                                      ; c3                          ; 0xc2d4f
  ; disGetNextSymbol 0xc2d50 LB 0x1533 -> off=0x0 cb=0000000000000007 uValue=00000000000c2d50 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2d50 LB 0x7
    push bp                                   ; 55                          ; 0xc2d50 vgabios.c:1941
    mov bp, sp                                ; 89 e5                       ; 0xc2d51
    pop bp                                    ; 5d                          ; 0xc2d53 vgabios.c:1947
    retn 00002h                               ; c2 02 00                    ; 0xc2d54
  ; disGetNextSymbol 0xc2d57 LB 0x152c -> off=0x0 cb=0000000000000005 uValue=00000000000c2d57 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2d57 LB 0x5
    push bp                                   ; 55                          ; 0xc2d57 vgabios.c:1948
    mov bp, sp                                ; 89 e5                       ; 0xc2d58
    pop bp                                    ; 5d                          ; 0xc2d5a vgabios.c:1953
    retn                                      ; c3                          ; 0xc2d5b
  ; disGetNextSymbol 0xc2d5c LB 0x1527 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d5c 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2d5c LB 0x5
    push bp                                   ; 55                          ; 0xc2d5c vgabios.c:1954
    mov bp, sp                                ; 89 e5                       ; 0xc2d5d
    pop bp                                    ; 5d                          ; 0xc2d5f vgabios.c:1959
    retn                                      ; c3                          ; 0xc2d60
  ; disGetNextSymbol 0xc2d61 LB 0x1522 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d61 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2d61 LB 0x5
    push bp                                   ; 55                          ; 0xc2d61 vgabios.c:1960
    mov bp, sp                                ; 89 e5                       ; 0xc2d62
    pop bp                                    ; 5d                          ; 0xc2d64 vgabios.c:1965
    retn                                      ; c3                          ; 0xc2d65
  ; disGetNextSymbol 0xc2d66 LB 0x151d -> off=0x0 cb=0000000000000005 uValue=00000000000c2d66 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2d66 LB 0x5
    push bp                                   ; 55                          ; 0xc2d66 vgabios.c:1967
    mov bp, sp                                ; 89 e5                       ; 0xc2d67
    pop bp                                    ; 5d                          ; 0xc2d69 vgabios.c:1972
    retn                                      ; c3                          ; 0xc2d6a
  ; disGetNextSymbol 0xc2d6b LB 0x1518 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d6b 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2d6b LB 0x5
    push bp                                   ; 55                          ; 0xc2d6b vgabios.c:1975
    mov bp, sp                                ; 89 e5                       ; 0xc2d6c
    pop bp                                    ; 5d                          ; 0xc2d6e vgabios.c:1980
    retn                                      ; c3                          ; 0xc2d6f
  ; disGetNextSymbol 0xc2d70 LB 0x1513 -> off=0x0 cb=0000000000000005 uValue=00000000000c2d70 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2d70 LB 0x5
    push bp                                   ; 55                          ; 0xc2d70 vgabios.c:1981
    mov bp, sp                                ; 89 e5                       ; 0xc2d71
    pop bp                                    ; 5d                          ; 0xc2d73 vgabios.c:1986
    retn                                      ; c3                          ; 0xc2d74
  ; disGetNextSymbol 0xc2d75 LB 0x150e -> off=0x0 cb=000000000000009d uValue=00000000000c2d75 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2d75 LB 0x9d
    push bp                                   ; 55                          ; 0xc2d75 vgabios.c:1989
    mov bp, sp                                ; 89 e5                       ; 0xc2d76
    push si                                   ; 56                          ; 0xc2d78
    push di                                   ; 57                          ; 0xc2d79
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2d7a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2d7d
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc2d80
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2d83
    mov si, cx                                ; 89 ce                       ; 0xc2d86
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2d88
    mov al, dl                                ; 88 d0                       ; 0xc2d8b vgabios.c:1996
    xor ah, ah                                ; 30 e4                       ; 0xc2d8d
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2d8f
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2d92
    call 00a0bh                               ; e8 73 dc                    ; 0xc2d95
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2d98 vgabios.c:1999
    jne short 02dafh                          ; 75 11                       ; 0xc2d9c
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2d9e vgabios.c:2000
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2da1
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2da4 vgabios.c:2001
    xor al, al                                ; 30 c0                       ; 0xc2da7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2da9
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2dac
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc2daf vgabios.c:2004
    xor dh, dh                                ; 30 f6                       ; 0xc2db2
    sal dx, 008h                              ; c1 e2 08                    ; 0xc2db4
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2db7
    xor ah, ah                                ; 30 e4                       ; 0xc2dba
    add dx, ax                                ; 01 c2                       ; 0xc2dbc
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2dbe vgabios.c:2005
    call 0120eh                               ; e8 4a e4                    ; 0xc2dc1
    dec si                                    ; 4e                          ; 0xc2dc4 vgabios.c:2007
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2dc5
    je short 02df8h                           ; 74 2e                       ; 0xc2dc8
    mov bx, di                                ; 89 fb                       ; 0xc2dca vgabios.c:2009
    inc di                                    ; 47                          ; 0xc2dcc
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2dcd vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2dd0
    test byte [bp-006h], 002h                 ; f6 46 fa 02                 ; 0xc2dd3 vgabios.c:2010
    je short 02de2h                           ; 74 09                       ; 0xc2dd7
    mov bx, di                                ; 89 fb                       ; 0xc2dd9 vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2ddb
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2ddc vgabios.c:37
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2ddf vgabios.c:38
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2de2 vgabios.c:2013
    xor bh, bh                                ; 30 ff                       ; 0xc2de5
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2de7
    xor dh, dh                                ; 30 f6                       ; 0xc2dea
    mov al, ah                                ; 88 e0                       ; 0xc2dec
    xor ah, ah                                ; 30 e4                       ; 0xc2dee
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2df0
    call 0280ch                               ; e8 16 fa                    ; 0xc2df3
    jmp short 02dc4h                          ; eb cc                       ; 0xc2df6 vgabios.c:2014
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc2df8 vgabios.c:2017
    jne short 02e09h                          ; 75 0b                       ; 0xc2dfc
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2dfe vgabios.c:2018
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2e01
    xor ah, ah                                ; 30 e4                       ; 0xc2e04
    call 0120eh                               ; e8 05 e4                    ; 0xc2e06
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2e09 vgabios.c:2019
    pop di                                    ; 5f                          ; 0xc2e0c
    pop si                                    ; 5e                          ; 0xc2e0d
    pop bp                                    ; 5d                          ; 0xc2e0e
    retn 00008h                               ; c2 08 00                    ; 0xc2e0f
  ; disGetNextSymbol 0xc2e12 LB 0x1471 -> off=0x0 cb=00000000000001f2 uValue=00000000000c2e12 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2e12 LB 0x1f2
    push bp                                   ; 55                          ; 0xc2e12 vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2e13
    push cx                                   ; 51                          ; 0xc2e15
    push si                                   ; 56                          ; 0xc2e16
    push di                                   ; 57                          ; 0xc2e17
    push ax                                   ; 50                          ; 0xc2e18
    push ax                                   ; 50                          ; 0xc2e19
    push dx                                   ; 52                          ; 0xc2e1a
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e1b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e1e
    mov es, ax                                ; 8e c0                       ; 0xc2e21
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e23
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2e26 vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2e29 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2e2c
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e2f vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2e32 vgabios.c:2033
    mov es, dx                                ; 8e c2                       ; 0xc2e34 vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2e36
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2e3b
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2e3f vgabios.c:2038
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2e42
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e45
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2e48
    cld                                       ; fc                          ; 0xc2e4b
    jcxz 02e54h                               ; e3 06                       ; 0xc2e4c
    push DS                                   ; 1e                          ; 0xc2e4e
    mov ds, dx                                ; 8e da                       ; 0xc2e4f
    rep movsb                                 ; f3 a4                       ; 0xc2e51
    pop DS                                    ; 1f                          ; 0xc2e53
    mov si, 00084h                            ; be 84 00                    ; 0xc2e54 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e57
    mov es, ax                                ; 8e c0                       ; 0xc2e5a
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e5c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2e5f vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2e61
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2e64 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2e67
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2e6a vgabios.c:2040
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2e6d
    mov si, 00085h                            ; be 85 00                    ; 0xc2e70
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2e73
    cld                                       ; fc                          ; 0xc2e76
    jcxz 02e7fh                               ; e3 06                       ; 0xc2e77
    push DS                                   ; 1e                          ; 0xc2e79
    mov ds, dx                                ; 8e da                       ; 0xc2e7a
    rep movsb                                 ; f3 a4                       ; 0xc2e7c
    pop DS                                    ; 1f                          ; 0xc2e7e
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2e7f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e82
    mov es, ax                                ; 8e c0                       ; 0xc2e85
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e87
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2e8a vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2e8d vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2e90
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2e93 vgabios.c:2043
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2e96 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2e9a vgabios.c:2044
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2e9d vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2ea2 vgabios.c:2045
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2ea5 vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2ea9 vgabios.c:2046
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2eac vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2eb0 vgabios.c:2047
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2eb3 vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2eb7 vgabios.c:2048
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2eba vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2ebe vgabios.c:2049
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2ec1 vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2ec5 vgabios.c:2050
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2ec8 vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2ecc vgabios.c:2051
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2ecf vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2ed3 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ed6
    mov es, ax                                ; 8e c0                       ; 0xc2ed9
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2edb
    mov dl, al                                ; 88 c2                       ; 0xc2ede vgabios.c:2056
    and dl, 080h                              ; 80 e2 80                    ; 0xc2ee0
    xor dh, dh                                ; 30 f6                       ; 0xc2ee3
    sar dx, 006h                              ; c1 fa 06                    ; 0xc2ee5
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2ee8
    xor ah, ah                                ; 30 e4                       ; 0xc2eea
    sar ax, 004h                              ; c1 f8 04                    ; 0xc2eec
    or ax, dx                                 ; 09 d0                       ; 0xc2eef
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2ef1 vgabios.c:2057
    je short 02f07h                           ; 74 11                       ; 0xc2ef4
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2ef6
    je short 02f03h                           ; 74 08                       ; 0xc2ef9
    test ax, ax                               ; 85 c0                       ; 0xc2efb
    jne short 02f07h                          ; 75 08                       ; 0xc2efd
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2eff vgabios.c:2058
    jmp short 02f09h                          ; eb 06                       ; 0xc2f01
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2f03 vgabios.c:2059
    jmp short 02f09h                          ; eb 02                       ; 0xc2f05
    xor al, al                                ; 30 c0                       ; 0xc2f07 vgabios.c:2061
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f09 vgabios.c:2063
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f0c vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f0f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f12 vgabios.c:2066
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2f15
    jc short 02f38h                           ; 72 1f                       ; 0xc2f17
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2f19
    jnbe short 02f38h                         ; 77 1b                       ; 0xc2f1b
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2f1d vgabios.c:2067
    test ax, ax                               ; 85 c0                       ; 0xc2f20
    je short 02f7ah                           ; 74 56                       ; 0xc2f22
    mov si, ax                                ; 89 c6                       ; 0xc2f24 vgabios.c:2068
    shr si, 002h                              ; c1 ee 02                    ; 0xc2f26
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2f29
    xor dx, dx                                ; 31 d2                       ; 0xc2f2c
    div si                                    ; f7 f6                       ; 0xc2f2e
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f30
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f33 vgabios.c:42
    jmp short 02f7ah                          ; eb 42                       ; 0xc2f36 vgabios.c:2069
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f38
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f3b
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2f3e
    jne short 02f53h                          ; 75 11                       ; 0xc2f40
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f42 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2f45
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f49 vgabios.c:2071
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2f4c vgabios.c:52
    jmp short 02f7ah                          ; eb 27                       ; 0xc2f51 vgabios.c:2072
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2f53
    jc short 02f7ah                           ; 72 23                       ; 0xc2f55
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2f57
    jnbe short 02f7ah                         ; 77 1f                       ; 0xc2f59
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2f5b vgabios.c:2074
    je short 02f6fh                           ; 74 0e                       ; 0xc2f5f
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2f61 vgabios.c:2075
    xor dx, dx                                ; 31 d2                       ; 0xc2f64
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2f66
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f69 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f6c
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f6f vgabios.c:2076
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f72 vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2f75
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f7a vgabios.c:2078
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2f7d
    je short 02f85h                           ; 74 04                       ; 0xc2f7f
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2f81
    jne short 02f90h                          ; 75 0b                       ; 0xc2f83
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f85 vgabios.c:2079
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f88 vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc2f8b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f90 vgabios.c:2081
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2f93
    jc short 02fech                           ; 72 55                       ; 0xc2f95
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc2f97
    je short 02fech                           ; 74 51                       ; 0xc2f99
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2f9b vgabios.c:2082
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f9e vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2fa1
    mov si, 00084h                            ; be 84 00                    ; 0xc2fa5 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fa8
    mov es, ax                                ; 8e c0                       ; 0xc2fab
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fad
    xor ah, ah                                ; 30 e4                       ; 0xc2fb0 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc2fb2
    mov si, 00085h                            ; be 85 00                    ; 0xc2fb3 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2fb6
    xor dh, dh                                ; 30 f6                       ; 0xc2fb9 vgabios.c:38
    imul dx                                   ; f7 ea                       ; 0xc2fbb
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc2fbd vgabios.c:2084
    jc short 02fd0h                           ; 72 0e                       ; 0xc2fc0
    jbe short 02fd9h                          ; 76 15                       ; 0xc2fc2
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc2fc4
    je short 02fe1h                           ; 74 18                       ; 0xc2fc7
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc2fc9
    je short 02fddh                           ; 74 0f                       ; 0xc2fcc
    jmp short 02fe1h                          ; eb 11                       ; 0xc2fce
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc2fd0
    jne short 02fe1h                          ; 75 0c                       ; 0xc2fd3
    xor al, al                                ; 30 c0                       ; 0xc2fd5 vgabios.c:2085
    jmp short 02fe3h                          ; eb 0a                       ; 0xc2fd7
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2fd9 vgabios.c:2086
    jmp short 02fe3h                          ; eb 06                       ; 0xc2fdb
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2fdd vgabios.c:2087
    jmp short 02fe3h                          ; eb 02                       ; 0xc2fdf
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2fe1 vgabios.c:2089
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2fe3 vgabios.c:2091
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fe6 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fe9
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc2fec vgabios.c:2094
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2fef
    xor ax, ax                                ; 31 c0                       ; 0xc2ff2
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2ff4
    cld                                       ; fc                          ; 0xc2ff7
    jcxz 02ffch                               ; e3 02                       ; 0xc2ff8
    rep stosb                                 ; f3 aa                       ; 0xc2ffa
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2ffc vgabios.c:2095
    pop di                                    ; 5f                          ; 0xc2fff
    pop si                                    ; 5e                          ; 0xc3000
    pop cx                                    ; 59                          ; 0xc3001
    pop bp                                    ; 5d                          ; 0xc3002
    retn                                      ; c3                          ; 0xc3003
  ; disGetNextSymbol 0xc3004 LB 0x127f -> off=0x0 cb=0000000000000023 uValue=00000000000c3004 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc3004 LB 0x23
    push dx                                   ; 52                          ; 0xc3004 vgabios.c:2098
    push bp                                   ; 55                          ; 0xc3005
    mov bp, sp                                ; 89 e5                       ; 0xc3006
    mov dx, ax                                ; 89 c2                       ; 0xc3008
    xor ax, ax                                ; 31 c0                       ; 0xc300a vgabios.c:2102
    test dl, 001h                             ; f6 c2 01                    ; 0xc300c vgabios.c:2103
    je short 03014h                           ; 74 03                       ; 0xc300f
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc3011 vgabios.c:2104
    test dl, 002h                             ; f6 c2 02                    ; 0xc3014 vgabios.c:2106
    je short 0301ch                           ; 74 03                       ; 0xc3017
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc3019 vgabios.c:2107
    test dl, 004h                             ; f6 c2 04                    ; 0xc301c vgabios.c:2109
    je short 03024h                           ; 74 03                       ; 0xc301f
    add ax, 00304h                            ; 05 04 03                    ; 0xc3021 vgabios.c:2110
    pop bp                                    ; 5d                          ; 0xc3024 vgabios.c:2113
    pop dx                                    ; 5a                          ; 0xc3025
    retn                                      ; c3                          ; 0xc3026
  ; disGetNextSymbol 0xc3027 LB 0x125c -> off=0x0 cb=0000000000000018 uValue=00000000000c3027 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3027 LB 0x18
    push bp                                   ; 55                          ; 0xc3027 vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc3028
    push bx                                   ; 53                          ; 0xc302a
    mov bx, dx                                ; 89 d3                       ; 0xc302b
    call 03004h                               ; e8 d4 ff                    ; 0xc302d vgabios.c:2118
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3030
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3033
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc3036
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3039 vgabios.c:2119
    pop bx                                    ; 5b                          ; 0xc303c
    pop bp                                    ; 5d                          ; 0xc303d
    retn                                      ; c3                          ; 0xc303e
  ; disGetNextSymbol 0xc303f LB 0x1244 -> off=0x0 cb=00000000000002d8 uValue=00000000000c303f 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc303f LB 0x2d8
    push bp                                   ; 55                          ; 0xc303f vgabios.c:2121
    mov bp, sp                                ; 89 e5                       ; 0xc3040
    push cx                                   ; 51                          ; 0xc3042
    push si                                   ; 56                          ; 0xc3043
    push di                                   ; 57                          ; 0xc3044
    push ax                                   ; 50                          ; 0xc3045
    push ax                                   ; 50                          ; 0xc3046
    push ax                                   ; 50                          ; 0xc3047
    mov cx, dx                                ; 89 d1                       ; 0xc3048
    mov si, strict word 00063h                ; be 63 00                    ; 0xc304a vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc304d
    mov es, ax                                ; 8e c0                       ; 0xc3050
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc3052
    mov si, di                                ; 89 fe                       ; 0xc3055 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc3057 vgabios.c:2126
    je short 030c3h                           ; 74 66                       ; 0xc305b
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc305d vgabios.c:2127
    in AL, DX                                 ; ec                          ; 0xc3060
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3061
    mov es, cx                                ; 8e c1                       ; 0xc3063 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3065
    inc bx                                    ; 43                          ; 0xc3068 vgabios.c:2127
    mov dx, di                                ; 89 fa                       ; 0xc3069
    in AL, DX                                 ; ec                          ; 0xc306b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc306c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc306e vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3071 vgabios.c:2128
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3072
    in AL, DX                                 ; ec                          ; 0xc3075
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3076
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3078 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc307b vgabios.c:2129
    mov dx, 003dah                            ; ba da 03                    ; 0xc307c
    in AL, DX                                 ; ec                          ; 0xc307f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3080
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3082 vgabios.c:2131
    in AL, DX                                 ; ec                          ; 0xc3085
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3086
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3088
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc308b vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc308e
    inc bx                                    ; 43                          ; 0xc3091 vgabios.c:2132
    mov dx, 003cah                            ; ba ca 03                    ; 0xc3092
    in AL, DX                                 ; ec                          ; 0xc3095
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3096
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3098 vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc309b vgabios.c:2135
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc309e
    add bx, ax                                ; 01 c3                       ; 0xc30a1 vgabios.c:2133
    jmp short 030abh                          ; eb 06                       ; 0xc30a3
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc30a5
    jnbe short 030c6h                         ; 77 1b                       ; 0xc30a9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc30ab vgabios.c:2136
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc30ae
    out DX, AL                                ; ee                          ; 0xc30b1
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc30b2 vgabios.c:2137
    in AL, DX                                 ; ec                          ; 0xc30b5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30b6
    mov es, cx                                ; 8e c1                       ; 0xc30b8 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ba
    inc bx                                    ; 43                          ; 0xc30bd vgabios.c:2137
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30be vgabios.c:2138
    jmp short 030a5h                          ; eb e2                       ; 0xc30c1
    jmp near 03173h                           ; e9 ad 00                    ; 0xc30c3
    xor al, al                                ; 30 c0                       ; 0xc30c6 vgabios.c:2139
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc30c8
    out DX, AL                                ; ee                          ; 0xc30cb
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc30cc vgabios.c:2140
    in AL, DX                                 ; ec                          ; 0xc30cf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30d0
    mov es, cx                                ; 8e c1                       ; 0xc30d2 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30d4
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc30d7 vgabios.c:2142
    inc bx                                    ; 43                          ; 0xc30dc vgabios.c:2140
    jmp short 030e5h                          ; eb 06                       ; 0xc30dd
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc30df
    jnbe short 030fch                         ; 77 17                       ; 0xc30e3
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc30e5 vgabios.c:2143
    mov dx, si                                ; 89 f2                       ; 0xc30e8
    out DX, AL                                ; ee                          ; 0xc30ea
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc30eb vgabios.c:2144
    in AL, DX                                 ; ec                          ; 0xc30ee
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30ef
    mov es, cx                                ; 8e c1                       ; 0xc30f1 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30f3
    inc bx                                    ; 43                          ; 0xc30f6 vgabios.c:2144
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30f7 vgabios.c:2145
    jmp short 030dfh                          ; eb e3                       ; 0xc30fa
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc30fc vgabios.c:2147
    jmp short 03109h                          ; eb 06                       ; 0xc3101
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3103
    jnbe short 0312dh                         ; 77 24                       ; 0xc3107
    mov dx, 003dah                            ; ba da 03                    ; 0xc3109 vgabios.c:2148
    in AL, DX                                 ; ec                          ; 0xc310c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc310d
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc310f vgabios.c:2149
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3112
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3115
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3118
    out DX, AL                                ; ee                          ; 0xc311b
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc311c vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc311f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3120
    mov es, cx                                ; 8e c1                       ; 0xc3122 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3124
    inc bx                                    ; 43                          ; 0xc3127 vgabios.c:2150
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3128 vgabios.c:2151
    jmp short 03103h                          ; eb d6                       ; 0xc312b
    mov dx, 003dah                            ; ba da 03                    ; 0xc312d vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc3130
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3131
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3133 vgabios.c:2154
    jmp short 03140h                          ; eb 06                       ; 0xc3138
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc313a
    jnbe short 03158h                         ; 77 18                       ; 0xc313e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3140 vgabios.c:2155
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3143
    out DX, AL                                ; ee                          ; 0xc3146
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc3147 vgabios.c:2156
    in AL, DX                                 ; ec                          ; 0xc314a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc314b
    mov es, cx                                ; 8e c1                       ; 0xc314d vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc314f
    inc bx                                    ; 43                          ; 0xc3152 vgabios.c:2156
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3153 vgabios.c:2157
    jmp short 0313ah                          ; eb e2                       ; 0xc3156
    mov es, cx                                ; 8e c1                       ; 0xc3158 vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc315a
    inc bx                                    ; 43                          ; 0xc315d vgabios.c:2159
    inc bx                                    ; 43                          ; 0xc315e
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc315f vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3163 vgabios.c:2162
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3164 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3168 vgabios.c:2163
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3169 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc316d vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc316e vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3172 vgabios.c:2165
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc3173 vgabios.c:2167
    jne short 0317ch                          ; 75 03                       ; 0xc3177
    jmp near 032bbh                           ; e9 3f 01                    ; 0xc3179
    mov si, strict word 00049h                ; be 49 00                    ; 0xc317c vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc317f
    mov es, ax                                ; 8e c0                       ; 0xc3182
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3184
    mov es, cx                                ; 8e c1                       ; 0xc3187 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3189
    inc bx                                    ; 43                          ; 0xc318c vgabios.c:2168
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc318d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3190
    mov es, ax                                ; 8e c0                       ; 0xc3193
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3195
    mov es, cx                                ; 8e c1                       ; 0xc3198 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc319a
    inc bx                                    ; 43                          ; 0xc319d vgabios.c:2169
    inc bx                                    ; 43                          ; 0xc319e
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc319f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31a2
    mov es, ax                                ; 8e c0                       ; 0xc31a5
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc31a7
    mov es, cx                                ; 8e c1                       ; 0xc31aa vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31ac
    inc bx                                    ; 43                          ; 0xc31af vgabios.c:2170
    inc bx                                    ; 43                          ; 0xc31b0
    mov si, strict word 00063h                ; be 63 00                    ; 0xc31b1 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31b4
    mov es, ax                                ; 8e c0                       ; 0xc31b7
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc31b9
    mov es, cx                                ; 8e c1                       ; 0xc31bc vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31be
    inc bx                                    ; 43                          ; 0xc31c1 vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc31c2
    mov si, 00084h                            ; be 84 00                    ; 0xc31c3 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31c6
    mov es, ax                                ; 8e c0                       ; 0xc31c9
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31cb
    mov es, cx                                ; 8e c1                       ; 0xc31ce vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31d0
    inc bx                                    ; 43                          ; 0xc31d3 vgabios.c:2172
    mov si, 00085h                            ; be 85 00                    ; 0xc31d4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31d7
    mov es, ax                                ; 8e c0                       ; 0xc31da
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc31dc
    mov es, cx                                ; 8e c1                       ; 0xc31df vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31e1
    inc bx                                    ; 43                          ; 0xc31e4 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc31e5
    mov si, 00087h                            ; be 87 00                    ; 0xc31e6 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31e9
    mov es, ax                                ; 8e c0                       ; 0xc31ec
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31ee
    mov es, cx                                ; 8e c1                       ; 0xc31f1 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31f3
    inc bx                                    ; 43                          ; 0xc31f6 vgabios.c:2174
    mov si, 00088h                            ; be 88 00                    ; 0xc31f7 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31fa
    mov es, ax                                ; 8e c0                       ; 0xc31fd
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31ff
    mov es, cx                                ; 8e c1                       ; 0xc3202 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3204
    inc bx                                    ; 43                          ; 0xc3207 vgabios.c:2175
    mov si, 00089h                            ; be 89 00                    ; 0xc3208 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc320b
    mov es, ax                                ; 8e c0                       ; 0xc320e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3210
    mov es, cx                                ; 8e c1                       ; 0xc3213 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3215
    inc bx                                    ; 43                          ; 0xc3218 vgabios.c:2176
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3219 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc321c
    mov es, ax                                ; 8e c0                       ; 0xc321f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3221
    mov es, cx                                ; 8e c1                       ; 0xc3224 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3226
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3229 vgabios.c:2178
    inc bx                                    ; 43                          ; 0xc322e vgabios.c:2177
    inc bx                                    ; 43                          ; 0xc322f
    jmp short 03238h                          ; eb 06                       ; 0xc3230
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3232
    jnc short 03254h                          ; 73 1c                       ; 0xc3236
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc3238 vgabios.c:2179
    add si, si                                ; 01 f6                       ; 0xc323b
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc323d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3240 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc3243
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3245
    mov es, cx                                ; 8e c1                       ; 0xc3248 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc324a
    inc bx                                    ; 43                          ; 0xc324d vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc324e
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc324f vgabios.c:2181
    jmp short 03232h                          ; eb de                       ; 0xc3252
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3254 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3257
    mov es, ax                                ; 8e c0                       ; 0xc325a
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc325c
    mov es, cx                                ; 8e c1                       ; 0xc325f vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3261
    inc bx                                    ; 43                          ; 0xc3264 vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc3265
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3266 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3269
    mov es, ax                                ; 8e c0                       ; 0xc326c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc326e
    mov es, cx                                ; 8e c1                       ; 0xc3271 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3273
    inc bx                                    ; 43                          ; 0xc3276 vgabios.c:2183
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3277 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc327a
    mov es, ax                                ; 8e c0                       ; 0xc327c
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc327e
    mov es, cx                                ; 8e c1                       ; 0xc3281 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3283
    inc bx                                    ; 43                          ; 0xc3286 vgabios.c:2185
    inc bx                                    ; 43                          ; 0xc3287
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc3288 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc328b
    mov es, ax                                ; 8e c0                       ; 0xc328d
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc328f
    mov es, cx                                ; 8e c1                       ; 0xc3292 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3294
    inc bx                                    ; 43                          ; 0xc3297 vgabios.c:2186
    inc bx                                    ; 43                          ; 0xc3298
    mov si, 0010ch                            ; be 0c 01                    ; 0xc3299 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc329c
    mov es, ax                                ; 8e c0                       ; 0xc329e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32a0
    mov es, cx                                ; 8e c1                       ; 0xc32a3 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32a5
    inc bx                                    ; 43                          ; 0xc32a8 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc32a9
    mov si, 0010eh                            ; be 0e 01                    ; 0xc32aa vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc32ad
    mov es, ax                                ; 8e c0                       ; 0xc32af
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32b1
    mov es, cx                                ; 8e c1                       ; 0xc32b4 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32b6
    inc bx                                    ; 43                          ; 0xc32b9 vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc32ba
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc32bb vgabios.c:2190
    je short 0330dh                           ; 74 4c                       ; 0xc32bf
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc32c1 vgabios.c:2192
    in AL, DX                                 ; ec                          ; 0xc32c4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32c5
    mov es, cx                                ; 8e c1                       ; 0xc32c7 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32c9
    inc bx                                    ; 43                          ; 0xc32cc vgabios.c:2192
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc32cd
    in AL, DX                                 ; ec                          ; 0xc32d0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32d1
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32d3 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc32d6 vgabios.c:2193
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc32d7
    in AL, DX                                 ; ec                          ; 0xc32da
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32db
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32dd vgabios.c:42
    inc bx                                    ; 43                          ; 0xc32e0 vgabios.c:2194
    xor al, al                                ; 30 c0                       ; 0xc32e1
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc32e3
    out DX, AL                                ; ee                          ; 0xc32e6
    xor ah, ah                                ; 30 e4                       ; 0xc32e7 vgabios.c:2197
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc32e9
    jmp short 032f5h                          ; eb 07                       ; 0xc32ec
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc32ee
    jnc short 03306h                          ; 73 11                       ; 0xc32f3
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc32f5 vgabios.c:2198
    in AL, DX                                 ; ec                          ; 0xc32f8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc32f9
    mov es, cx                                ; 8e c1                       ; 0xc32fb vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32fd
    inc bx                                    ; 43                          ; 0xc3300 vgabios.c:2198
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3301 vgabios.c:2199
    jmp short 032eeh                          ; eb e8                       ; 0xc3304
    mov es, cx                                ; 8e c1                       ; 0xc3306 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3308
    inc bx                                    ; 43                          ; 0xc330c vgabios.c:2200
    mov ax, bx                                ; 89 d8                       ; 0xc330d vgabios.c:2203
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc330f
    pop di                                    ; 5f                          ; 0xc3312
    pop si                                    ; 5e                          ; 0xc3313
    pop cx                                    ; 59                          ; 0xc3314
    pop bp                                    ; 5d                          ; 0xc3315
    retn                                      ; c3                          ; 0xc3316
  ; disGetNextSymbol 0xc3317 LB 0xf6c -> off=0x0 cb=00000000000002ba uValue=00000000000c3317 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3317 LB 0x2ba
    push bp                                   ; 55                          ; 0xc3317 vgabios.c:2205
    mov bp, sp                                ; 89 e5                       ; 0xc3318
    push cx                                   ; 51                          ; 0xc331a
    push si                                   ; 56                          ; 0xc331b
    push di                                   ; 57                          ; 0xc331c
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc331d
    push ax                                   ; 50                          ; 0xc3320
    mov cx, dx                                ; 89 d1                       ; 0xc3321
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3323 vgabios.c:2209
    je short 03380h                           ; 74 57                       ; 0xc3327
    mov dx, 003dah                            ; ba da 03                    ; 0xc3329 vgabios.c:2211
    in AL, DX                                 ; ec                          ; 0xc332c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc332d
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc332f vgabios.c:2213
    mov es, cx                                ; 8e c1                       ; 0xc3332 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3334
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3337 vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc333a vgabios.c:2214
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc333c vgabios.c:2217
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc3341 vgabios.c:2215
    jmp short 0334ch                          ; eb 06                       ; 0xc3344
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3346
    jnbe short 03362h                         ; 77 16                       ; 0xc334a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc334c vgabios.c:2218
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc334f
    out DX, AL                                ; ee                          ; 0xc3352
    mov es, cx                                ; 8e c1                       ; 0xc3353 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3355
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3358 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc335b
    inc bx                                    ; 43                          ; 0xc335c vgabios.c:2219
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc335d vgabios.c:2220
    jmp short 03346h                          ; eb e4                       ; 0xc3360
    xor al, al                                ; 30 c0                       ; 0xc3362 vgabios.c:2221
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3364
    out DX, AL                                ; ee                          ; 0xc3367
    mov es, cx                                ; 8e c1                       ; 0xc3368 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc336a
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc336d vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3370
    inc bx                                    ; 43                          ; 0xc3371 vgabios.c:2222
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3372
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3375
    out DX, ax                                ; ef                          ; 0xc3378
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3379 vgabios.c:2227
    jmp short 03389h                          ; eb 09                       ; 0xc337e
    jmp near 03460h                           ; e9 dd 00                    ; 0xc3380
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc3383
    jnbe short 033a3h                         ; 77 1a                       ; 0xc3387
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc3389 vgabios.c:2228
    je short 0339dh                           ; 74 0e                       ; 0xc338d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc338f vgabios.c:2229
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3392
    out DX, AL                                ; ee                          ; 0xc3395
    mov es, cx                                ; 8e c1                       ; 0xc3396 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3398
    inc dx                                    ; 42                          ; 0xc339b vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc339c
    inc bx                                    ; 43                          ; 0xc339d vgabios.c:2232
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc339e vgabios.c:2233
    jmp short 03383h                          ; eb e0                       ; 0xc33a1
    mov dx, 003cch                            ; ba cc 03                    ; 0xc33a3 vgabios.c:2235
    in AL, DX                                 ; ec                          ; 0xc33a6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33a7
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc33a9
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc33ab
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc33ae vgabios.c:2236
    jne short 033b9h                          ; 75 04                       ; 0xc33b3
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc33b5 vgabios.c:2237
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc33b9 vgabios.c:2238
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc33bc
    out DX, AL                                ; ee                          ; 0xc33bf
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc33c0 vgabios.c:2241
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc33c2
    out DX, AL                                ; ee                          ; 0xc33c5
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc33c6 vgabios.c:2242
    mov es, cx                                ; 8e c1                       ; 0xc33ca vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc33cc
    inc dx                                    ; 42                          ; 0xc33cf vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33d0
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc33d1 vgabios.c:2245
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc33d4 vgabios.c:37
    xor dh, dh                                ; 30 f6                       ; 0xc33d7 vgabios.c:38
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc33d9
    mov dx, 003dah                            ; ba da 03                    ; 0xc33dc vgabios.c:2246
    in AL, DX                                 ; ec                          ; 0xc33df
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33e0
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33e2 vgabios.c:2247
    jmp short 033efh                          ; eb 06                       ; 0xc33e7
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc33e9
    jnbe short 03408h                         ; 77 19                       ; 0xc33ed
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc33ef vgabios.c:2248
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc33f2
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc33f5
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc33f8
    out DX, AL                                ; ee                          ; 0xc33fb
    mov es, cx                                ; 8e c1                       ; 0xc33fc vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33fe
    out DX, AL                                ; ee                          ; 0xc3401 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc3402 vgabios.c:2249
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3403 vgabios.c:2250
    jmp short 033e9h                          ; eb e1                       ; 0xc3406
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3408 vgabios.c:2251
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc340b
    out DX, AL                                ; ee                          ; 0xc340e
    mov dx, 003dah                            ; ba da 03                    ; 0xc340f vgabios.c:2252
    in AL, DX                                 ; ec                          ; 0xc3412
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3413
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3415 vgabios.c:2254
    jmp short 03422h                          ; eb 06                       ; 0xc341a
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc341c
    jnbe short 03438h                         ; 77 16                       ; 0xc3420
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3422 vgabios.c:2255
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3425
    out DX, AL                                ; ee                          ; 0xc3428
    mov es, cx                                ; 8e c1                       ; 0xc3429 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc342b
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc342e vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3431
    inc bx                                    ; 43                          ; 0xc3432 vgabios.c:2256
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3433 vgabios.c:2257
    jmp short 0341ch                          ; eb e4                       ; 0xc3436
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc3438 vgabios.c:2258
    mov es, cx                                ; 8e c1                       ; 0xc343b vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc343d
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3440 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3443
    inc si                                    ; 46                          ; 0xc3444 vgabios.c:2261
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3445 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3448 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc344b
    inc si                                    ; 46                          ; 0xc344c vgabios.c:2262
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc344d vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3450 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3453
    inc si                                    ; 46                          ; 0xc3454 vgabios.c:2263
    inc si                                    ; 46                          ; 0xc3455
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3456 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3459 vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc345c
    out DX, AL                                ; ee                          ; 0xc345f
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3460 vgabios.c:2267
    jne short 03469h                          ; 75 03                       ; 0xc3464
    jmp near 03584h                           ; e9 1b 01                    ; 0xc3466
    mov es, cx                                ; 8e c1                       ; 0xc3469 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc346b
    mov si, strict word 00049h                ; be 49 00                    ; 0xc346e vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3471
    mov es, dx                                ; 8e c2                       ; 0xc3474
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3476
    inc bx                                    ; 43                          ; 0xc3479 vgabios.c:2268
    mov es, cx                                ; 8e c1                       ; 0xc347a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc347c
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc347f vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3482
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3484
    inc bx                                    ; 43                          ; 0xc3487 vgabios.c:2269
    inc bx                                    ; 43                          ; 0xc3488
    mov es, cx                                ; 8e c1                       ; 0xc3489 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc348b
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc348e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3491
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3493
    inc bx                                    ; 43                          ; 0xc3496 vgabios.c:2270
    inc bx                                    ; 43                          ; 0xc3497
    mov es, cx                                ; 8e c1                       ; 0xc3498 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc349a
    mov si, strict word 00063h                ; be 63 00                    ; 0xc349d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34a0
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34a2
    inc bx                                    ; 43                          ; 0xc34a5 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc34a6
    mov es, cx                                ; 8e c1                       ; 0xc34a7 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34a9
    mov si, 00084h                            ; be 84 00                    ; 0xc34ac vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34af
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34b1
    inc bx                                    ; 43                          ; 0xc34b4 vgabios.c:2272
    mov es, cx                                ; 8e c1                       ; 0xc34b5 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34b7
    mov si, 00085h                            ; be 85 00                    ; 0xc34ba vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34bd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34bf
    inc bx                                    ; 43                          ; 0xc34c2 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc34c3
    mov es, cx                                ; 8e c1                       ; 0xc34c4 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34c6
    mov si, 00087h                            ; be 87 00                    ; 0xc34c9 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34cc
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34ce
    inc bx                                    ; 43                          ; 0xc34d1 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc34d2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34d4
    mov si, 00088h                            ; be 88 00                    ; 0xc34d7 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34da
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34dc
    inc bx                                    ; 43                          ; 0xc34df vgabios.c:2275
    mov es, cx                                ; 8e c1                       ; 0xc34e0 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34e2
    mov si, 00089h                            ; be 89 00                    ; 0xc34e5 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc34e8
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34ea
    inc bx                                    ; 43                          ; 0xc34ed vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc34ee vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34f0
    mov si, strict word 00060h                ; be 60 00                    ; 0xc34f3 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34f6
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34f8
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc34fb vgabios.c:2278
    inc bx                                    ; 43                          ; 0xc3500 vgabios.c:2277
    inc bx                                    ; 43                          ; 0xc3501
    jmp short 0350ah                          ; eb 06                       ; 0xc3502
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3504
    jnc short 03526h                          ; 73 1c                       ; 0xc3508
    mov es, cx                                ; 8e c1                       ; 0xc350a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc350c
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc350f vgabios.c:48
    add si, si                                ; 01 f6                       ; 0xc3512
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3514
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3517 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc351a
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc351c
    inc bx                                    ; 43                          ; 0xc351f vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc3520
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3521 vgabios.c:2281
    jmp short 03504h                          ; eb de                       ; 0xc3524
    mov es, cx                                ; 8e c1                       ; 0xc3526 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3528
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc352b vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc352e
    mov es, dx                                ; 8e c2                       ; 0xc3531
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3533
    inc bx                                    ; 43                          ; 0xc3536 vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc3537
    mov es, cx                                ; 8e c1                       ; 0xc3538 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc353a
    mov si, strict word 00062h                ; be 62 00                    ; 0xc353d vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3540
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3542
    inc bx                                    ; 43                          ; 0xc3545 vgabios.c:2283
    mov es, cx                                ; 8e c1                       ; 0xc3546 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3548
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc354b vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc354e
    mov es, dx                                ; 8e c2                       ; 0xc3550
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3552
    inc bx                                    ; 43                          ; 0xc3555 vgabios.c:2285
    inc bx                                    ; 43                          ; 0xc3556
    mov es, cx                                ; 8e c1                       ; 0xc3557 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3559
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc355c vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc355f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3561
    inc bx                                    ; 43                          ; 0xc3564 vgabios.c:2286
    inc bx                                    ; 43                          ; 0xc3565
    mov es, cx                                ; 8e c1                       ; 0xc3566 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3568
    mov si, 0010ch                            ; be 0c 01                    ; 0xc356b vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc356e
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3570
    inc bx                                    ; 43                          ; 0xc3573 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc3574
    mov es, cx                                ; 8e c1                       ; 0xc3575 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3577
    mov si, 0010eh                            ; be 0e 01                    ; 0xc357a vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc357d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc357f
    inc bx                                    ; 43                          ; 0xc3582 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc3583
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3584 vgabios.c:2290
    je short 035c7h                           ; 74 3d                       ; 0xc3588
    inc bx                                    ; 43                          ; 0xc358a vgabios.c:2291
    mov es, cx                                ; 8e c1                       ; 0xc358b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc358d
    xor ah, ah                                ; 30 e4                       ; 0xc3590 vgabios.c:38
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3592
    inc bx                                    ; 43                          ; 0xc3595 vgabios.c:2292
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3596 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3599 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc359c
    inc bx                                    ; 43                          ; 0xc359d vgabios.c:2293
    xor al, al                                ; 30 c0                       ; 0xc359e
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35a0
    out DX, AL                                ; ee                          ; 0xc35a3
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc35a4 vgabios.c:2296
    jmp short 035b0h                          ; eb 07                       ; 0xc35a7
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc35a9
    jnc short 035bfh                          ; 73 0f                       ; 0xc35ae
    mov es, cx                                ; 8e c1                       ; 0xc35b0 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35b2
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc35b5 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc35b8
    inc bx                                    ; 43                          ; 0xc35b9 vgabios.c:2297
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc35ba vgabios.c:2298
    jmp short 035a9h                          ; eb ea                       ; 0xc35bd
    inc bx                                    ; 43                          ; 0xc35bf vgabios.c:2299
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc35c0
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35c3
    out DX, AL                                ; ee                          ; 0xc35c6
    mov ax, bx                                ; 89 d8                       ; 0xc35c7 vgabios.c:2303
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc35c9
    pop di                                    ; 5f                          ; 0xc35cc
    pop si                                    ; 5e                          ; 0xc35cd
    pop cx                                    ; 59                          ; 0xc35ce
    pop bp                                    ; 5d                          ; 0xc35cf
    retn                                      ; c3                          ; 0xc35d0
  ; disGetNextSymbol 0xc35d1 LB 0xcb2 -> off=0x0 cb=0000000000000028 uValue=00000000000c35d1 'find_vga_entry'
find_vga_entry:                              ; 0xc35d1 LB 0x28
    push bx                                   ; 53                          ; 0xc35d1 vgabios.c:2312
    push dx                                   ; 52                          ; 0xc35d2
    push bp                                   ; 55                          ; 0xc35d3
    mov bp, sp                                ; 89 e5                       ; 0xc35d4
    mov dl, al                                ; 88 c2                       ; 0xc35d6
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc35d8 vgabios.c:2314
    xor al, al                                ; 30 c0                       ; 0xc35da vgabios.c:2315
    jmp short 035e4h                          ; eb 06                       ; 0xc35dc
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc35de vgabios.c:2316
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc35e0
    jnbe short 035f3h                         ; 77 0f                       ; 0xc35e2
    mov bl, al                                ; 88 c3                       ; 0xc35e4
    xor bh, bh                                ; 30 ff                       ; 0xc35e6
    sal bx, 003h                              ; c1 e3 03                    ; 0xc35e8
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc35eb
    jne short 035deh                          ; 75 ed                       ; 0xc35ef
    mov ah, al                                ; 88 c4                       ; 0xc35f1
    mov al, ah                                ; 88 e0                       ; 0xc35f3 vgabios.c:2321
    pop bp                                    ; 5d                          ; 0xc35f5
    pop dx                                    ; 5a                          ; 0xc35f6
    pop bx                                    ; 5b                          ; 0xc35f7
    retn                                      ; c3                          ; 0xc35f8
  ; disGetNextSymbol 0xc35f9 LB 0xc8a -> off=0x0 cb=000000000000000e uValue=00000000000c35f9 'xread_byte'
xread_byte:                                  ; 0xc35f9 LB 0xe
    push bx                                   ; 53                          ; 0xc35f9 vgabios.c:2333
    push bp                                   ; 55                          ; 0xc35fa
    mov bp, sp                                ; 89 e5                       ; 0xc35fb
    mov bx, dx                                ; 89 d3                       ; 0xc35fd
    mov es, ax                                ; 8e c0                       ; 0xc35ff vgabios.c:2335
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3601
    pop bp                                    ; 5d                          ; 0xc3604 vgabios.c:2336
    pop bx                                    ; 5b                          ; 0xc3605
    retn                                      ; c3                          ; 0xc3606
  ; disGetNextSymbol 0xc3607 LB 0xc7c -> off=0x87 cb=0000000000000451 uValue=00000000000c368e 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0d8h, 03ah, 0b7h, 036h, 0f4h, 036h, 007h, 037h, 017h, 037h
    db  02ah, 037h, 03ah, 037h, 044h, 037h, 086h, 037h, 0bah, 037h, 0cbh, 037h, 0e6h, 037h, 00ch, 038h
    db  02bh, 038h, 042h, 038h, 058h, 038h, 064h, 038h, 02eh, 039h, 09bh, 039h, 0c8h, 039h, 0ddh, 039h
    db  01fh, 03ah, 0aah, 03ah, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 0d8h, 03ah, 083h, 038h, 0a4h, 038h, 0b3h, 038h, 0c2h, 038h, 083h, 038h, 0a4h, 038h
    db  0b3h, 038h, 0c2h, 038h, 0d1h, 038h, 0ddh, 038h, 0f8h, 038h, 002h, 039h, 00ch, 039h, 016h, 039h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 09ch, 03ah, 045h, 03ah, 053h, 03ah, 064h, 03ah, 074h
    db  03ah, 089h, 03ah, 09ch, 03ah, 09ch, 03ah
int10_func:                                  ; 0xc368e LB 0x451
    push bp                                   ; 55                          ; 0xc368e vgabios.c:2414
    mov bp, sp                                ; 89 e5                       ; 0xc368f
    push si                                   ; 56                          ; 0xc3691
    push di                                   ; 57                          ; 0xc3692
    push ax                                   ; 50                          ; 0xc3693
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3694
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3697 vgabios.c:2419
    shr ax, 008h                              ; c1 e8 08                    ; 0xc369a
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc369d
    jnbe short 03704h                         ; 77 62                       ; 0xc36a0
    push CS                                   ; 0e                          ; 0xc36a2
    pop ES                                    ; 07                          ; 0xc36a3
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc36a4
    mov di, 03607h                            ; bf 07 36                    ; 0xc36a7
    repne scasb                               ; f2 ae                       ; 0xc36aa
    sal cx, 1                                 ; d1 e1                       ; 0xc36ac
    mov di, cx                                ; 89 cf                       ; 0xc36ae
    mov ax, word [cs:di+0361dh]               ; 2e 8b 85 1d 36              ; 0xc36b0
    jmp ax                                    ; ff e0                       ; 0xc36b5
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc36b7 vgabios.c:2422
    xor ah, ah                                ; 30 e4                       ; 0xc36ba
    call 01375h                               ; e8 b6 dc                    ; 0xc36bc
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36bf vgabios.c:2423
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc36c2
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc36c5
    je short 036dfh                           ; 74 15                       ; 0xc36c8
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc36ca
    je short 036d6h                           ; 74 07                       ; 0xc36cd
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc36cf
    jbe short 036dfh                          ; 76 0b                       ; 0xc36d2
    jmp short 036e8h                          ; eb 12                       ; 0xc36d4
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36d6 vgabios.c:2425
    xor al, al                                ; 30 c0                       ; 0xc36d9
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc36db
    jmp short 036efh                          ; eb 10                       ; 0xc36dd vgabios.c:2426
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36df vgabios.c:2434
    xor al, al                                ; 30 c0                       ; 0xc36e2
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc36e4
    jmp short 036efh                          ; eb 07                       ; 0xc36e6
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36e8 vgabios.c:2437
    xor al, al                                ; 30 c0                       ; 0xc36eb
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc36ed
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc36ef
    jmp short 03704h                          ; eb 10                       ; 0xc36f2 vgabios.c:2439
    mov dl, byte [bp+010h]                    ; 8a 56 10                    ; 0xc36f4 vgabios.c:2441
    xor dh, dh                                ; 30 f6                       ; 0xc36f7
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc36f9
    shr ax, 008h                              ; c1 e8 08                    ; 0xc36fc
    xor ah, ah                                ; 30 e4                       ; 0xc36ff
    call 01107h                               ; e8 03 da                    ; 0xc3701
    jmp near 03ad8h                           ; e9 d1 03                    ; 0xc3704 vgabios.c:2442
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc3707 vgabios.c:2444
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc370a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc370d
    xor ah, ah                                ; 30 e4                       ; 0xc3710
    call 0120eh                               ; e8 f9 da                    ; 0xc3712
    jmp short 03704h                          ; eb ed                       ; 0xc3715 vgabios.c:2445
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3717 vgabios.c:2447
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc371a
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc371d
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3720
    xor ah, ah                                ; 30 e4                       ; 0xc3723
    call 00a0bh                               ; e8 e3 d2                    ; 0xc3725
    jmp short 03704h                          ; eb da                       ; 0xc3728 vgabios.c:2448
    xor ax, ax                                ; 31 c0                       ; 0xc372a vgabios.c:2454
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc372c
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc372f vgabios.c:2455
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3732 vgabios.c:2456
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3735 vgabios.c:2457
    jmp short 03704h                          ; eb ca                       ; 0xc3738 vgabios.c:2458
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc373a vgabios.c:2460
    xor ah, ah                                ; 30 e4                       ; 0xc373d
    call 0129dh                               ; e8 5b db                    ; 0xc373f
    jmp short 03704h                          ; eb c0                       ; 0xc3742 vgabios.c:2461
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3744 vgabios.c:2463
    push ax                                   ; 50                          ; 0xc3747
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3748
    push ax                                   ; 50                          ; 0xc374b
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc374c
    xor ah, ah                                ; 30 e4                       ; 0xc374f
    push ax                                   ; 50                          ; 0xc3751
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3752
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3755
    xor ah, ah                                ; 30 e4                       ; 0xc3758
    push ax                                   ; 50                          ; 0xc375a
    mov cl, byte [bp+010h]                    ; 8a 4e 10                    ; 0xc375b
    xor ch, ch                                ; 30 ed                       ; 0xc375e
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3760
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3763
    mov bl, al                                ; 88 c3                       ; 0xc3766
    xor bh, bh                                ; 30 ff                       ; 0xc3768
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc376a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc376d
    mov dl, al                                ; 88 c2                       ; 0xc3770
    xor dh, dh                                ; 30 f6                       ; 0xc3772
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3774
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3777
    mov byte [bp-005h], bh                    ; 88 7e fb                    ; 0xc377a
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc377d
    call 01a42h                               ; e8 bf e2                    ; 0xc3780
    jmp near 03ad8h                           ; e9 52 03                    ; 0xc3783 vgabios.c:2464
    xor ax, ax                                ; 31 c0                       ; 0xc3786 vgabios.c:2466
    push ax                                   ; 50                          ; 0xc3788
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3789
    push ax                                   ; 50                          ; 0xc378c
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc378d
    xor ah, ah                                ; 30 e4                       ; 0xc3790
    push ax                                   ; 50                          ; 0xc3792
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3793
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3796
    xor ah, ah                                ; 30 e4                       ; 0xc3799
    push ax                                   ; 50                          ; 0xc379b
    mov cl, byte [bp+010h]                    ; 8a 4e 10                    ; 0xc379c
    xor ch, ch                                ; 30 ed                       ; 0xc379f
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37a1
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37a4
    xor ah, ah                                ; 30 e4                       ; 0xc37a7
    mov bx, ax                                ; 89 c3                       ; 0xc37a9
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37ab
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37ae
    xor ah, ah                                ; 30 e4                       ; 0xc37b1
    mov dx, ax                                ; 89 c2                       ; 0xc37b3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37b5
    jmp short 03780h                          ; eb c6                       ; 0xc37b8
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc37ba vgabios.c:2469
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37bd
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37c0
    xor ah, ah                                ; 30 e4                       ; 0xc37c3
    call 00d4bh                               ; e8 83 d5                    ; 0xc37c5
    jmp near 03ad8h                           ; e9 0d 03                    ; 0xc37c8 vgabios.c:2470
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc37cb vgabios.c:2472
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc37ce
    xor bh, bh                                ; 30 ff                       ; 0xc37d1
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37d3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37d6
    xor ah, ah                                ; 30 e4                       ; 0xc37d9
    mov dx, ax                                ; 89 c2                       ; 0xc37db
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37dd
    call 02390h                               ; e8 ad eb                    ; 0xc37e0
    jmp near 03ad8h                           ; e9 f2 02                    ; 0xc37e3 vgabios.c:2473
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc37e6 vgabios.c:2475
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc37e9
    xor bh, bh                                ; 30 ff                       ; 0xc37ec
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37ee
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37f1
    xor ah, ah                                ; 30 e4                       ; 0xc37f4
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc37f6
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc37f9
    mov byte [bp-005h], bh                    ; 88 7e fb                    ; 0xc37fc
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc37ff
    mov dx, ax                                ; 89 c2                       ; 0xc3802
    mov ax, si                                ; 89 f0                       ; 0xc3804
    call 02518h                               ; e8 0f ed                    ; 0xc3806
    jmp near 03ad8h                           ; e9 cc 02                    ; 0xc3809 vgabios.c:2476
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc380c vgabios.c:2478
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc380f
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc3812
    xor dh, dh                                ; 30 f6                       ; 0xc3815
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3817
    shr ax, 008h                              ; c1 e8 08                    ; 0xc381a
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc381d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3820
    xor ah, ah                                ; 30 e4                       ; 0xc3823
    call 02699h                               ; e8 71 ee                    ; 0xc3825
    jmp near 03ad8h                           ; e9 ad 02                    ; 0xc3828 vgabios.c:2479
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc382b vgabios.c:2481
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc382e
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3831
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3834
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3837
    xor ah, ah                                ; 30 e4                       ; 0xc383a
    call 00f14h                               ; e8 d5 d6                    ; 0xc383c
    jmp near 03ad8h                           ; e9 96 02                    ; 0xc383f vgabios.c:2482
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3842 vgabios.c:2490
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3845
    xor ah, ah                                ; 30 e4                       ; 0xc3848
    mov bx, ax                                ; 89 c3                       ; 0xc384a
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc384c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc384f
    call 0280ch                               ; e8 b7 ef                    ; 0xc3852
    jmp near 03ad8h                           ; e9 80 02                    ; 0xc3855 vgabios.c:2491
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3858 vgabios.c:2494
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc385b
    call 0107ah                               ; e8 19 d8                    ; 0xc385e
    jmp near 03ad8h                           ; e9 74 02                    ; 0xc3861 vgabios.c:2495
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3864 vgabios.c:2497
    xor ah, ah                                ; 30 e4                       ; 0xc3867
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3869
    jnbe short 038dah                         ; 77 6c                       ; 0xc386c
    push CS                                   ; 0e                          ; 0xc386e
    pop ES                                    ; 07                          ; 0xc386f
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc3870
    mov di, 0364bh                            ; bf 4b 36                    ; 0xc3873
    repne scasb                               ; f2 ae                       ; 0xc3876
    sal cx, 1                                 ; d1 e1                       ; 0xc3878
    mov di, cx                                ; 89 cf                       ; 0xc387a
    mov ax, word [cs:di+03659h]               ; 2e 8b 85 59 36              ; 0xc387c
    jmp ax                                    ; ff e0                       ; 0xc3881
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3883 vgabios.c:2501
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3886
    xor ah, ah                                ; 30 e4                       ; 0xc3889
    push ax                                   ; 50                          ; 0xc388b
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc388c
    push ax                                   ; 50                          ; 0xc388f
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3890
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3893
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3896
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3899
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc389c
    call 02b7dh                               ; e8 db f2                    ; 0xc389f
    jmp short 038dah                          ; eb 36                       ; 0xc38a2 vgabios.c:2502
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38a4 vgabios.c:2505
    xor dh, dh                                ; 30 f6                       ; 0xc38a7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38a9
    xor ah, ah                                ; 30 e4                       ; 0xc38ac
    call 02bfdh                               ; e8 4c f3                    ; 0xc38ae
    jmp short 038dah                          ; eb 27                       ; 0xc38b1 vgabios.c:2506
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38b3 vgabios.c:2509
    xor dh, dh                                ; 30 f6                       ; 0xc38b6
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38b8
    xor ah, ah                                ; 30 e4                       ; 0xc38bb
    call 02c6bh                               ; e8 ab f3                    ; 0xc38bd
    jmp short 038dah                          ; eb 18                       ; 0xc38c0 vgabios.c:2510
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38c2 vgabios.c:2513
    xor dh, dh                                ; 30 f6                       ; 0xc38c5
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38c7
    xor ah, ah                                ; 30 e4                       ; 0xc38ca
    call 02cdbh                               ; e8 0c f4                    ; 0xc38cc
    jmp short 038dah                          ; eb 09                       ; 0xc38cf vgabios.c:2514
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38d1 vgabios.c:2516
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc38d4
    call 02d4bh                               ; e8 71 f4                    ; 0xc38d7
    jmp near 03ad8h                           ; e9 fb 01                    ; 0xc38da vgabios.c:2517
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc38dd vgabios.c:2519
    xor ah, ah                                ; 30 e4                       ; 0xc38e0
    push ax                                   ; 50                          ; 0xc38e2
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38e3
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc38e6
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38e9
    mov si, word [bp+016h]                    ; 8b 76 16                    ; 0xc38ec
    mov cx, ax                                ; 89 c1                       ; 0xc38ef
    mov ax, si                                ; 89 f0                       ; 0xc38f1
    call 02d50h                               ; e8 5a f4                    ; 0xc38f3
    jmp short 038dah                          ; eb e2                       ; 0xc38f6 vgabios.c:2520
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38f8 vgabios.c:2522
    xor ah, ah                                ; 30 e4                       ; 0xc38fb
    call 02d57h                               ; e8 57 f4                    ; 0xc38fd
    jmp short 038dah                          ; eb d8                       ; 0xc3900 vgabios.c:2523
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3902 vgabios.c:2525
    xor ah, ah                                ; 30 e4                       ; 0xc3905
    call 02d5ch                               ; e8 52 f4                    ; 0xc3907
    jmp short 038dah                          ; eb ce                       ; 0xc390a vgabios.c:2526
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc390c vgabios.c:2528
    xor ah, ah                                ; 30 e4                       ; 0xc390f
    call 02d61h                               ; e8 4d f4                    ; 0xc3911
    jmp short 038dah                          ; eb c4                       ; 0xc3914 vgabios.c:2529
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc3916 vgabios.c:2531
    push ax                                   ; 50                          ; 0xc3919
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc391a
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc391d
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3920
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3923
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3926
    call 00e8bh                               ; e8 5f d5                    ; 0xc3929
    jmp short 038dah                          ; eb ac                       ; 0xc392c vgabios.c:2539
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc392e vgabios.c:2541
    xor ah, ah                                ; 30 e4                       ; 0xc3931
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc3933
    jc short 03946h                           ; 72 0e                       ; 0xc3936
    jbe short 03950h                          ; 76 16                       ; 0xc3938
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc393a
    je short 03993h                           ; 74 54                       ; 0xc393d
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc393f
    je short 03985h                           ; 74 41                       ; 0xc3942
    jmp short 038dah                          ; eb 94                       ; 0xc3944
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc3946
    jne short 03982h                          ; 75 37                       ; 0xc3949
    call 02d66h                               ; e8 18 f4                    ; 0xc394b vgabios.c:2544
    jmp short 03982h                          ; eb 32                       ; 0xc394e vgabios.c:2545
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3950 vgabios.c:2547
    xor ah, ah                                ; 30 e4                       ; 0xc3953
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3955
    jnc short 0397fh                          ; 73 25                       ; 0xc3958
    mov dx, 00087h                            ; ba 87 00                    ; 0xc395a vgabios.c:2548
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc395d
    call 035f9h                               ; e8 96 fc                    ; 0xc3960
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3963
    mov ah, byte [bp+012h]                    ; 8a 66 12                    ; 0xc3965
    or al, ah                                 ; 08 e0                       ; 0xc3968
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc396a vgabios.c:40
    mov es, dx                                ; 8e c2                       ; 0xc396d
    mov si, 00087h                            ; be 87 00                    ; 0xc396f
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3972 vgabios.c:42
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3975 vgabios.c:2550
    xor al, al                                ; 30 c0                       ; 0xc3978
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc397a
    jmp near 036efh                           ; e9 70 fd                    ; 0xc397c
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc397f vgabios.c:2553
    jmp near 03ad8h                           ; e9 53 01                    ; 0xc3982 vgabios.c:2554
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3985 vgabios.c:2556
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3988
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc398b
    call 02d6bh                               ; e8 da f3                    ; 0xc398e
    jmp short 03975h                          ; eb e2                       ; 0xc3991
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3993 vgabios.c:2560
    call 02d70h                               ; e8 d7 f3                    ; 0xc3996
    jmp short 03975h                          ; eb da                       ; 0xc3999
    push word [bp+008h]                       ; ff 76 08                    ; 0xc399b vgabios.c:2570
    push word [bp+016h]                       ; ff 76 16                    ; 0xc399e
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39a1
    xor ah, ah                                ; 30 e4                       ; 0xc39a4
    push ax                                   ; 50                          ; 0xc39a6
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc39a7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc39aa
    xor ah, ah                                ; 30 e4                       ; 0xc39ad
    push ax                                   ; 50                          ; 0xc39af
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc39b0
    xor bh, bh                                ; 30 ff                       ; 0xc39b3
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc39b5
    shr dx, 008h                              ; c1 ea 08                    ; 0xc39b8
    xor dh, dh                                ; 30 f6                       ; 0xc39bb
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39bd
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc39c0
    call 02d75h                               ; e8 af f3                    ; 0xc39c3
    jmp short 03982h                          ; eb ba                       ; 0xc39c6 vgabios.c:2571
    mov bx, si                                ; 89 f3                       ; 0xc39c8 vgabios.c:2573
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39ca
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39cd
    call 02e12h                               ; e8 3f f4                    ; 0xc39d0
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39d3 vgabios.c:2574
    xor al, al                                ; 30 c0                       ; 0xc39d6
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc39d8
    jmp near 036efh                           ; e9 12 fd                    ; 0xc39da
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39dd vgabios.c:2577
    xor ah, ah                                ; 30 e4                       ; 0xc39e0
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc39e2
    je short 03a09h                           ; 74 22                       ; 0xc39e5
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc39e7
    je short 039fbh                           ; 74 0f                       ; 0xc39ea
    test ax, ax                               ; 85 c0                       ; 0xc39ec
    jne short 03a15h                          ; 75 25                       ; 0xc39ee
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc39f0 vgabios.c:2580
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39f3
    call 03027h                               ; e8 2e f6                    ; 0xc39f6
    jmp short 03a15h                          ; eb 1a                       ; 0xc39f9 vgabios.c:2581
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc39fb vgabios.c:2583
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39fe
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a01
    call 0303fh                               ; e8 38 f6                    ; 0xc3a04
    jmp short 03a15h                          ; eb 0c                       ; 0xc3a07 vgabios.c:2584
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3a09 vgabios.c:2586
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a0c
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a0f
    call 03317h                               ; e8 02 f9                    ; 0xc3a12
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a15 vgabios.c:2593
    xor al, al                                ; 30 c0                       ; 0xc3a18
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3a1a
    jmp near 036efh                           ; e9 d0 fc                    ; 0xc3a1c
    call 007bfh                               ; e8 9d cd                    ; 0xc3a1f vgabios.c:2598
    test ax, ax                               ; 85 c0                       ; 0xc3a22
    je short 03a9ah                           ; 74 74                       ; 0xc3a24
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a26 vgabios.c:2599
    xor ah, ah                                ; 30 e4                       ; 0xc3a29
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3a2b
    jnbe short 03a9ch                         ; 77 6c                       ; 0xc3a2e
    push CS                                   ; 0e                          ; 0xc3a30
    pop ES                                    ; 07                          ; 0xc3a31
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3a32
    mov di, 03677h                            ; bf 77 36                    ; 0xc3a35
    repne scasb                               ; f2 ae                       ; 0xc3a38
    sal cx, 1                                 ; d1 e1                       ; 0xc3a3a
    mov di, cx                                ; 89 cf                       ; 0xc3a3c
    mov ax, word [cs:di+0367eh]               ; 2e 8b 85 7e 36              ; 0xc3a3e
    jmp ax                                    ; ff e0                       ; 0xc3a43
    mov bx, si                                ; 89 f3                       ; 0xc3a45 vgabios.c:2602
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a47
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a4a
    call 03c92h                               ; e8 42 02                    ; 0xc3a4d
    jmp near 03ad8h                           ; e9 85 00                    ; 0xc3a50 vgabios.c:2603
    mov cx, si                                ; 89 f1                       ; 0xc3a53 vgabios.c:2605
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a55
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a58
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a5b
    call 03dbdh                               ; e8 5c 03                    ; 0xc3a5e
    jmp near 03ad8h                           ; e9 74 00                    ; 0xc3a61 vgabios.c:2606
    mov cx, si                                ; 89 f1                       ; 0xc3a64 vgabios.c:2608
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a66
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3a69
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a6c
    call 03e5dh                               ; e8 eb 03                    ; 0xc3a6f
    jmp short 03ad8h                          ; eb 64                       ; 0xc3a72 vgabios.c:2609
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3a74 vgabios.c:2611
    push ax                                   ; 50                          ; 0xc3a77
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3a78
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3a7b
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a7e
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a81
    call 04026h                               ; e8 9f 05                    ; 0xc3a84
    jmp short 03ad8h                          ; eb 4f                       ; 0xc3a87 vgabios.c:2612
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3a89 vgabios.c:2614
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3a8c
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3a8f
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a92
    call 040b2h                               ; e8 1a 06                    ; 0xc3a95
    jmp short 03ad8h                          ; eb 3e                       ; 0xc3a98 vgabios.c:2615
    jmp short 03aa3h                          ; eb 07                       ; 0xc3a9a
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3a9c vgabios.c:2637
    jmp short 03ad8h                          ; eb 35                       ; 0xc3aa1 vgabios.c:2640
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3aa3 vgabios.c:2642
    jmp short 03ad8h                          ; eb 2e                       ; 0xc3aa8 vgabios.c:2644
    call 007bfh                               ; e8 12 cd                    ; 0xc3aaa vgabios.c:2646
    test ax, ax                               ; 85 c0                       ; 0xc3aad
    je short 03ad3h                           ; 74 22                       ; 0xc3aaf
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3ab1 vgabios.c:2647
    xor ah, ah                                ; 30 e4                       ; 0xc3ab4
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3ab6
    jne short 03acch                          ; 75 11                       ; 0xc3ab9
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3abb vgabios.c:2650
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3abe
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3ac1
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3ac4
    call 04191h                               ; e8 c7 06                    ; 0xc3ac7
    jmp short 03ad8h                          ; eb 0c                       ; 0xc3aca vgabios.c:2651
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3acc vgabios.c:2653
    jmp short 03ad8h                          ; eb 05                       ; 0xc3ad1 vgabios.c:2656
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ad3 vgabios.c:2658
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ad8 vgabios.c:2668
    pop di                                    ; 5f                          ; 0xc3adb
    pop si                                    ; 5e                          ; 0xc3adc
    pop bp                                    ; 5d                          ; 0xc3add
    retn                                      ; c3                          ; 0xc3ade
  ; disGetNextSymbol 0xc3adf LB 0x7a4 -> off=0x0 cb=000000000000001f uValue=00000000000c3adf 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3adf LB 0x1f
    push bp                                   ; 55                          ; 0xc3adf vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3ae0
    push bx                                   ; 53                          ; 0xc3ae2
    push dx                                   ; 52                          ; 0xc3ae3
    mov bx, ax                                ; 89 c3                       ; 0xc3ae4
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3ae6 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ae9
    call 00570h                               ; e8 81 ca                    ; 0xc3aec
    mov ax, bx                                ; 89 d8                       ; 0xc3aef vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3af1
    call 00570h                               ; e8 79 ca                    ; 0xc3af4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3af7 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3afa
    pop bx                                    ; 5b                          ; 0xc3afb
    pop bp                                    ; 5d                          ; 0xc3afc
    retn                                      ; c3                          ; 0xc3afd
  ; disGetNextSymbol 0xc3afe LB 0x785 -> off=0x0 cb=000000000000001f uValue=00000000000c3afe 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3afe LB 0x1f
    push bp                                   ; 55                          ; 0xc3afe vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3aff
    push bx                                   ; 53                          ; 0xc3b01
    push dx                                   ; 52                          ; 0xc3b02
    mov bx, ax                                ; 89 c3                       ; 0xc3b03
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b05 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b08
    call 00570h                               ; e8 62 ca                    ; 0xc3b0b
    mov ax, bx                                ; 89 d8                       ; 0xc3b0e vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b10
    call 00570h                               ; e8 5a ca                    ; 0xc3b13
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b16 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3b19
    pop bx                                    ; 5b                          ; 0xc3b1a
    pop bp                                    ; 5d                          ; 0xc3b1b
    retn                                      ; c3                          ; 0xc3b1c
  ; disGetNextSymbol 0xc3b1d LB 0x766 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b1d 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3b1d LB 0x19
    push bp                                   ; 55                          ; 0xc3b1d vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3b1e
    push dx                                   ; 52                          ; 0xc3b20
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b21 vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b24
    call 00570h                               ; e8 46 ca                    ; 0xc3b27
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b2a vbe.c:121
    call 00577h                               ; e8 47 ca                    ; 0xc3b2d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b30 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3b33
    pop bp                                    ; 5d                          ; 0xc3b34
    retn                                      ; c3                          ; 0xc3b35
  ; disGetNextSymbol 0xc3b36 LB 0x74d -> off=0x0 cb=000000000000001f uValue=00000000000c3b36 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3b36 LB 0x1f
    push bp                                   ; 55                          ; 0xc3b36 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3b37
    push bx                                   ; 53                          ; 0xc3b39
    push dx                                   ; 52                          ; 0xc3b3a
    mov bx, ax                                ; 89 c3                       ; 0xc3b3b
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b3d vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b40
    call 00570h                               ; e8 2a ca                    ; 0xc3b43
    mov ax, bx                                ; 89 d8                       ; 0xc3b46 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b48
    call 00570h                               ; e8 22 ca                    ; 0xc3b4b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b4e vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3b51
    pop bx                                    ; 5b                          ; 0xc3b52
    pop bp                                    ; 5d                          ; 0xc3b53
    retn                                      ; c3                          ; 0xc3b54
  ; disGetNextSymbol 0xc3b55 LB 0x72e -> off=0x0 cb=0000000000000019 uValue=00000000000c3b55 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3b55 LB 0x19
    push bp                                   ; 55                          ; 0xc3b55 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3b56
    push dx                                   ; 52                          ; 0xc3b58
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b59 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b5c
    call 00570h                               ; e8 0e ca                    ; 0xc3b5f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b62 vbe.c:136
    call 00577h                               ; e8 0f ca                    ; 0xc3b65
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b68 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3b6b
    pop bp                                    ; 5d                          ; 0xc3b6c
    retn                                      ; c3                          ; 0xc3b6d
  ; disGetNextSymbol 0xc3b6e LB 0x715 -> off=0x0 cb=000000000000001f uValue=00000000000c3b6e 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3b6e LB 0x1f
    push bp                                   ; 55                          ; 0xc3b6e vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3b6f
    push bx                                   ; 53                          ; 0xc3b71
    push dx                                   ; 52                          ; 0xc3b72
    mov bx, ax                                ; 89 c3                       ; 0xc3b73
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b75 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b78
    call 00570h                               ; e8 f2 c9                    ; 0xc3b7b
    mov ax, bx                                ; 89 d8                       ; 0xc3b7e vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b80
    call 00570h                               ; e8 ea c9                    ; 0xc3b83
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b86 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3b89
    pop bx                                    ; 5b                          ; 0xc3b8a
    pop bp                                    ; 5d                          ; 0xc3b8b
    retn                                      ; c3                          ; 0xc3b8c
  ; disGetNextSymbol 0xc3b8d LB 0x6f6 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b8d 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3b8d LB 0x19
    push bp                                   ; 55                          ; 0xc3b8d vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3b8e
    push dx                                   ; 52                          ; 0xc3b90
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b91 vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b94
    call 00570h                               ; e8 d6 c9                    ; 0xc3b97
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b9a vbe.c:151
    call 00577h                               ; e8 d7 c9                    ; 0xc3b9d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ba0 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3ba3
    pop bp                                    ; 5d                          ; 0xc3ba4
    retn                                      ; c3                          ; 0xc3ba5
  ; disGetNextSymbol 0xc3ba6 LB 0x6dd -> off=0x0 cb=0000000000000019 uValue=00000000000c3ba6 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3ba6 LB 0x19
    push bp                                   ; 55                          ; 0xc3ba6 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3ba7
    push dx                                   ; 52                          ; 0xc3ba9
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3baa vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bad
    call 00570h                               ; e8 bd c9                    ; 0xc3bb0
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bb3 vbe.c:157
    call 00577h                               ; e8 be c9                    ; 0xc3bb6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bb9 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3bbc
    pop bp                                    ; 5d                          ; 0xc3bbd
    retn                                      ; c3                          ; 0xc3bbe
  ; disGetNextSymbol 0xc3bbf LB 0x6c4 -> off=0x0 cb=0000000000000012 uValue=00000000000c3bbf 'in_word'
in_word:                                     ; 0xc3bbf LB 0x12
    push bp                                   ; 55                          ; 0xc3bbf vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3bc0
    push bx                                   ; 53                          ; 0xc3bc2
    mov bx, ax                                ; 89 c3                       ; 0xc3bc3
    mov ax, dx                                ; 89 d0                       ; 0xc3bc5
    mov dx, bx                                ; 89 da                       ; 0xc3bc7 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3bc9
    in ax, DX                                 ; ed                          ; 0xc3bca vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bcb vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3bce
    pop bp                                    ; 5d                          ; 0xc3bcf
    retn                                      ; c3                          ; 0xc3bd0
  ; disGetNextSymbol 0xc3bd1 LB 0x6b2 -> off=0x0 cb=0000000000000014 uValue=00000000000c3bd1 'in_byte'
in_byte:                                     ; 0xc3bd1 LB 0x14
    push bp                                   ; 55                          ; 0xc3bd1 vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3bd2
    push bx                                   ; 53                          ; 0xc3bd4
    mov bx, ax                                ; 89 c3                       ; 0xc3bd5
    mov ax, dx                                ; 89 d0                       ; 0xc3bd7
    mov dx, bx                                ; 89 da                       ; 0xc3bd9 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3bdb
    in AL, DX                                 ; ec                          ; 0xc3bdc vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3bdd
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bdf vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3be2
    pop bp                                    ; 5d                          ; 0xc3be3
    retn                                      ; c3                          ; 0xc3be4
  ; disGetNextSymbol 0xc3be5 LB 0x69e -> off=0x0 cb=0000000000000014 uValue=00000000000c3be5 'dispi_get_id'
dispi_get_id:                                ; 0xc3be5 LB 0x14
    push bp                                   ; 55                          ; 0xc3be5 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3be6
    push dx                                   ; 52                          ; 0xc3be8
    xor ax, ax                                ; 31 c0                       ; 0xc3be9 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3beb
    out DX, ax                                ; ef                          ; 0xc3bee
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bef vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3bf2
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bf3 vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3bf6
    pop bp                                    ; 5d                          ; 0xc3bf7
    retn                                      ; c3                          ; 0xc3bf8
  ; disGetNextSymbol 0xc3bf9 LB 0x68a -> off=0x0 cb=000000000000001a uValue=00000000000c3bf9 'dispi_set_id'
dispi_set_id:                                ; 0xc3bf9 LB 0x1a
    push bp                                   ; 55                          ; 0xc3bf9 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3bfa
    push bx                                   ; 53                          ; 0xc3bfc
    push dx                                   ; 52                          ; 0xc3bfd
    mov bx, ax                                ; 89 c3                       ; 0xc3bfe
    xor ax, ax                                ; 31 c0                       ; 0xc3c00 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c02
    out DX, ax                                ; ef                          ; 0xc3c05
    mov ax, bx                                ; 89 d8                       ; 0xc3c06 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c08
    out DX, ax                                ; ef                          ; 0xc3c0b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c0c vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3c0f
    pop bx                                    ; 5b                          ; 0xc3c10
    pop bp                                    ; 5d                          ; 0xc3c11
    retn                                      ; c3                          ; 0xc3c12
  ; disGetNextSymbol 0xc3c13 LB 0x670 -> off=0x0 cb=000000000000002a uValue=00000000000c3c13 'vbe_init'
vbe_init:                                    ; 0xc3c13 LB 0x2a
    push bp                                   ; 55                          ; 0xc3c13 vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3c14
    push bx                                   ; 53                          ; 0xc3c16
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3c17 vbe.c:190
    call 03bf9h                               ; e8 dc ff                    ; 0xc3c1a
    call 03be5h                               ; e8 c5 ff                    ; 0xc3c1d vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3c20
    jne short 03c37h                          ; 75 12                       ; 0xc3c23
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3c25 vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3c28
    mov es, ax                                ; 8e c0                       ; 0xc3c2b
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3c2d
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3c31 vbe.c:194
    call 03bf9h                               ; e8 c2 ff                    ; 0xc3c34
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c37 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3c3a
    pop bp                                    ; 5d                          ; 0xc3c3b
    retn                                      ; c3                          ; 0xc3c3c
  ; disGetNextSymbol 0xc3c3d LB 0x646 -> off=0x0 cb=0000000000000055 uValue=00000000000c3c3d 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3c3d LB 0x55
    push bp                                   ; 55                          ; 0xc3c3d vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3c3e
    push bx                                   ; 53                          ; 0xc3c40
    push cx                                   ; 51                          ; 0xc3c41
    push si                                   ; 56                          ; 0xc3c42
    push di                                   ; 57                          ; 0xc3c43
    mov di, ax                                ; 89 c7                       ; 0xc3c44
    mov si, dx                                ; 89 d6                       ; 0xc3c46
    xor dx, dx                                ; 31 d2                       ; 0xc3c48 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c4a
    call 03bbfh                               ; e8 6f ff                    ; 0xc3c4d
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3c50 vbe.c:209
    jne short 03c87h                          ; 75 32                       ; 0xc3c53
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3c55 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3c58 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c5a
    call 03bbfh                               ; e8 5f ff                    ; 0xc3c5d
    mov cx, ax                                ; 89 c1                       ; 0xc3c60
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3c62 vbe.c:219
    je short 03c87h                           ; 74 20                       ; 0xc3c65
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3c67 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c6a
    call 03bbfh                               ; e8 4f ff                    ; 0xc3c6d
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3c70
    cmp cx, di                                ; 39 f9                       ; 0xc3c73 vbe.c:223
    jne short 03c83h                          ; 75 0c                       ; 0xc3c75
    test si, si                               ; 85 f6                       ; 0xc3c77 vbe.c:225
    jne short 03c7fh                          ; 75 04                       ; 0xc3c79
    mov ax, bx                                ; 89 d8                       ; 0xc3c7b vbe.c:226
    jmp short 03c89h                          ; eb 0a                       ; 0xc3c7d
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3c7f vbe.c:227
    jne short 03c7bh                          ; 75 f8                       ; 0xc3c81
    mov bx, dx                                ; 89 d3                       ; 0xc3c83 vbe.c:230
    jmp short 03c5ah                          ; eb d3                       ; 0xc3c85 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3c87 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3c89 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3c8c
    pop si                                    ; 5e                          ; 0xc3c8d
    pop cx                                    ; 59                          ; 0xc3c8e
    pop bx                                    ; 5b                          ; 0xc3c8f
    pop bp                                    ; 5d                          ; 0xc3c90
    retn                                      ; c3                          ; 0xc3c91
  ; disGetNextSymbol 0xc3c92 LB 0x5f1 -> off=0x0 cb=000000000000012b uValue=00000000000c3c92 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3c92 LB 0x12b
    push bp                                   ; 55                          ; 0xc3c92 vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3c93
    push cx                                   ; 51                          ; 0xc3c95
    push si                                   ; 56                          ; 0xc3c96
    push di                                   ; 57                          ; 0xc3c97
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3c98
    mov si, ax                                ; 89 c6                       ; 0xc3c9b
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3c9d
    mov di, bx                                ; 89 df                       ; 0xc3ca0
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3ca2 vbe.c:275
    call 005b7h                               ; e8 0d c9                    ; 0xc3ca7 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3caa
    mov bx, di                                ; 89 fb                       ; 0xc3cad vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3caf
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3cb2
    xor dx, dx                                ; 31 d2                       ; 0xc3cb5 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3cb7
    call 03bbfh                               ; e8 02 ff                    ; 0xc3cba
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3cbd vbe.c:285
    je short 03ccch                           ; 74 0a                       ; 0xc3cc0
    push SS                                   ; 16                          ; 0xc3cc2 vbe.c:287
    pop ES                                    ; 07                          ; 0xc3cc3
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3cc4
    jmp near 03db5h                           ; e9 e9 00                    ; 0xc3cc9 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3ccc vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3ccf vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3cd4 vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3cd7
    jne short 03ce6h                          ; 75 07                       ; 0xc3cdd
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3cdf
    je short 03cf5h                           ; 74 0f                       ; 0xc3ce4
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3ce6
    jne short 03cfah                          ; 75 0c                       ; 0xc3cec
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3cee
    jne short 03cfah                          ; 75 05                       ; 0xc3cf3
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3cf5 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3cfa vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3cfd
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc3d02 vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3d08 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3d0e vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3d14
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc3d18 vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3d1e vbe.c:332
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3d24 vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3d27
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3d2b vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3d2e
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3d32 vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d35
    call 03bbfh                               ; e8 84 fe                    ; 0xc3d38
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d3b
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3d3e
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3d42 vbe.c:342
    je short 03d6ch                           ; 74 24                       ; 0xc3d46
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3d48 vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3d4e vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3d54
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3d58 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3d5e
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3d62 vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3d68
    mov dx, cx                                ; 89 ca                       ; 0xc3d6c vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3d6e
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d71
    call 03bd1h                               ; e8 5a fe                    ; 0xc3d74
    xor ah, ah                                ; 30 e4                       ; 0xc3d77 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3d79
    jnbe short 03d95h                         ; 77 17                       ; 0xc3d7c
    mov dx, cx                                ; 89 ca                       ; 0xc3d7e vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d80
    call 03bbfh                               ; e8 39 fe                    ; 0xc3d83
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3d86 vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3d89
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3d8b vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d8e
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3d91 vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3d95 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3d98 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d9a
    call 03bbfh                               ; e8 1f fe                    ; 0xc3d9d
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3da0 vbe.c:368
    jne short 03d6ch                          ; 75 c7                       ; 0xc3da3
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3da5 vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3da8 vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3dab
    push SS                                   ; 16                          ; 0xc3dae vbe.c:372
    pop ES                                    ; 07                          ; 0xc3daf
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3db0
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3db5 vbe.c:373
    pop di                                    ; 5f                          ; 0xc3db8
    pop si                                    ; 5e                          ; 0xc3db9
    pop cx                                    ; 59                          ; 0xc3dba
    pop bp                                    ; 5d                          ; 0xc3dbb
    retn                                      ; c3                          ; 0xc3dbc
  ; disGetNextSymbol 0xc3dbd LB 0x4c6 -> off=0x0 cb=00000000000000a0 uValue=00000000000c3dbd 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3dbd LB 0xa0
    push bp                                   ; 55                          ; 0xc3dbd vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3dbe
    push si                                   ; 56                          ; 0xc3dc0
    push di                                   ; 57                          ; 0xc3dc1
    push ax                                   ; 50                          ; 0xc3dc2
    push ax                                   ; 50                          ; 0xc3dc3
    mov ax, dx                                ; 89 d0                       ; 0xc3dc4
    mov si, bx                                ; 89 de                       ; 0xc3dc6
    mov bx, cx                                ; 89 cb                       ; 0xc3dc8
    test dh, 040h                             ; f6 c6 40                    ; 0xc3dca vbe.c:396
    je short 03dd4h                           ; 74 05                       ; 0xc3dcd
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc3dcf
    jmp short 03dd6h                          ; eb 02                       ; 0xc3dd2
    xor dx, dx                                ; 31 d2                       ; 0xc3dd4
    and ah, 001h                              ; 80 e4 01                    ; 0xc3dd6 vbe.c:397
    call 03c3dh                               ; e8 61 fe                    ; 0xc3dd9 vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3ddc
    test ax, ax                               ; 85 c0                       ; 0xc3ddf vbe.c:401
    je short 03e4bh                           ; 74 68                       ; 0xc3de1
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3de3 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3de6
    mov di, bx                                ; 89 df                       ; 0xc3de8
    mov es, si                                ; 8e c6                       ; 0xc3dea
    cld                                       ; fc                          ; 0xc3dec
    jcxz 03df1h                               ; e3 02                       ; 0xc3ded
    rep stosb                                 ; f3 aa                       ; 0xc3def
    xor cx, cx                                ; 31 c9                       ; 0xc3df1 vbe.c:407
    jmp short 03dfah                          ; eb 05                       ; 0xc3df3
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3df5
    jnc short 03e13h                          ; 73 19                       ; 0xc3df8
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3dfa vbe.c:410
    inc dx                                    ; 42                          ; 0xc3dfd
    inc dx                                    ; 42                          ; 0xc3dfe
    add dx, cx                                ; 01 ca                       ; 0xc3dff
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e01
    call 03bd1h                               ; e8 ca fd                    ; 0xc3e04
    mov di, bx                                ; 89 df                       ; 0xc3e07 vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3e09
    mov es, si                                ; 8e c6                       ; 0xc3e0b vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3e0d
    inc cx                                    ; 41                          ; 0xc3e10 vbe.c:412
    jmp short 03df5h                          ; eb e2                       ; 0xc3e11
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3e13 vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3e16 vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3e18
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3e1b vbe.c:414
    je short 03e2fh                           ; 74 10                       ; 0xc3e1d
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3e1f vbe.c:415
    mov word [es:di], 00629h                  ; 26 c7 05 29 06              ; 0xc3e22 vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3e27 vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3e2a vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3e2f vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e32
    call 00570h                               ; e8 38 c7                    ; 0xc3e35
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e38 vbe.c:421
    call 00577h                               ; e8 39 c7                    ; 0xc3e3b
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3e3e
    mov es, si                                ; 8e c6                       ; 0xc3e41 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e43
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3e46 vbe.c:423
    jmp short 03e4eh                          ; eb 03                       ; 0xc3e49 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3e4b vbe.c:428
    push SS                                   ; 16                          ; 0xc3e4e vbe.c:431
    pop ES                                    ; 07                          ; 0xc3e4f
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3e50
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e53
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e56 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3e59
    pop si                                    ; 5e                          ; 0xc3e5a
    pop bp                                    ; 5d                          ; 0xc3e5b
    retn                                      ; c3                          ; 0xc3e5c
  ; disGetNextSymbol 0xc3e5d LB 0x426 -> off=0x0 cb=00000000000000e7 uValue=00000000000c3e5d 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3e5d LB 0xe7
    push bp                                   ; 55                          ; 0xc3e5d vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3e5e
    push si                                   ; 56                          ; 0xc3e60
    push di                                   ; 57                          ; 0xc3e61
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3e62
    mov si, ax                                ; 89 c6                       ; 0xc3e65
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3e67
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3e6a vbe.c:452
    je short 03e75h                           ; 74 05                       ; 0xc3e6e
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e70
    jmp short 03e77h                          ; eb 02                       ; 0xc3e73
    xor ax, ax                                ; 31 c0                       ; 0xc3e75
    mov dx, ax                                ; 89 c2                       ; 0xc3e77
    test ax, ax                               ; 85 c0                       ; 0xc3e79 vbe.c:453
    je short 03e80h                           ; 74 03                       ; 0xc3e7b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3e7d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3e80
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3e83 vbe.c:454
    je short 03e8eh                           ; 74 05                       ; 0xc3e87
    mov ax, 00080h                            ; b8 80 00                    ; 0xc3e89
    jmp short 03e90h                          ; eb 02                       ; 0xc3e8c
    xor ax, ax                                ; 31 c0                       ; 0xc3e8e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3e90
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3e93 vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3e97 vbe.c:459
    jnc short 03eb1h                          ; 73 13                       ; 0xc3e9c
    xor ax, ax                                ; 31 c0                       ; 0xc3e9e vbe.c:463
    call 005ddh                               ; e8 3a c7                    ; 0xc3ea0
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3ea3 vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc3ea6
    call 01375h                               ; e8 ca d4                    ; 0xc3ea8
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3eab vbe.c:468
    jmp near 03f38h                           ; e9 87 00                    ; 0xc3eae vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3eb1 vbe.c:472
    call 03c3dh                               ; e8 86 fd                    ; 0xc3eb4
    mov bx, ax                                ; 89 c3                       ; 0xc3eb7
    test ax, ax                               ; 85 c0                       ; 0xc3eb9 vbe.c:474
    je short 03f35h                           ; 74 78                       ; 0xc3ebb
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3ebd vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ec0
    call 03bbfh                               ; e8 f9 fc                    ; 0xc3ec3
    mov cx, ax                                ; 89 c1                       ; 0xc3ec6
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3ec8 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ecb
    call 03bbfh                               ; e8 ee fc                    ; 0xc3ece
    mov di, ax                                ; 89 c7                       ; 0xc3ed1
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3ed3 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ed6
    call 03bd1h                               ; e8 f5 fc                    ; 0xc3ed9
    mov bl, al                                ; 88 c3                       ; 0xc3edc
    mov dl, al                                ; 88 c2                       ; 0xc3ede
    xor ax, ax                                ; 31 c0                       ; 0xc3ee0 vbe.c:489
    call 005ddh                               ; e8 f8 c6                    ; 0xc3ee2
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3ee5 vbe.c:491
    jne short 03ef0h                          ; 75 06                       ; 0xc3ee8
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3eea vbe.c:493
    call 01375h                               ; e8 85 d4                    ; 0xc3eed
    mov al, dl                                ; 88 d0                       ; 0xc3ef0 vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3ef2
    call 03b36h                               ; e8 3f fc                    ; 0xc3ef4
    mov ax, cx                                ; 89 c8                       ; 0xc3ef7 vbe.c:497
    call 03adfh                               ; e8 e3 fb                    ; 0xc3ef9
    mov ax, di                                ; 89 f8                       ; 0xc3efc vbe.c:498
    call 03afeh                               ; e8 fd fb                    ; 0xc3efe
    xor ax, ax                                ; 31 c0                       ; 0xc3f01 vbe.c:499
    call 00603h                               ; e8 fd c6                    ; 0xc3f03
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3f06 vbe.c:500
    or dl, 001h                               ; 80 ca 01                    ; 0xc3f09
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3f0c
    xor ah, ah                                ; 30 e4                       ; 0xc3f0f
    or al, dl                                 ; 08 d0                       ; 0xc3f11
    call 005ddh                               ; e8 c7 c6                    ; 0xc3f13
    call 006d2h                               ; e8 b9 c7                    ; 0xc3f16 vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3f19 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f1c
    mov es, ax                                ; 8e c0                       ; 0xc3f1f
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3f21
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f24
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3f27 vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3f2a
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3f2c vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3f2f
    jmp near 03eabh                           ; e9 76 ff                    ; 0xc3f32
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3f35 vbe.c:513
    push SS                                   ; 16                          ; 0xc3f38 vbe.c:517
    pop ES                                    ; 07                          ; 0xc3f39
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3f3a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f3d vbe.c:518
    pop di                                    ; 5f                          ; 0xc3f40
    pop si                                    ; 5e                          ; 0xc3f41
    pop bp                                    ; 5d                          ; 0xc3f42
    retn                                      ; c3                          ; 0xc3f43
  ; disGetNextSymbol 0xc3f44 LB 0x33f -> off=0x0 cb=0000000000000008 uValue=00000000000c3f44 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3f44 LB 0x8
    push bp                                   ; 55                          ; 0xc3f44 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3f45
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3f47 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3f4a
    retn                                      ; c3                          ; 0xc3f4b
  ; disGetNextSymbol 0xc3f4c LB 0x337 -> off=0x0 cb=000000000000004b uValue=00000000000c3f4c 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3f4c LB 0x4b
    push bp                                   ; 55                          ; 0xc3f4c vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3f4d
    push bx                                   ; 53                          ; 0xc3f4f
    push cx                                   ; 51                          ; 0xc3f50
    push si                                   ; 56                          ; 0xc3f51
    mov si, ax                                ; 89 c6                       ; 0xc3f52
    mov bx, dx                                ; 89 d3                       ; 0xc3f54
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3f56 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f59
    out DX, ax                                ; ef                          ; 0xc3f5c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f5d vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3f60
    mov es, si                                ; 8e c6                       ; 0xc3f61 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f63
    inc bx                                    ; 43                          ; 0xc3f66 vbe.c:532
    inc bx                                    ; 43                          ; 0xc3f67
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3f68 vbe.c:533
    je short 03f8fh                           ; 74 23                       ; 0xc3f6a
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3f6c vbe.c:535
    jmp short 03f76h                          ; eb 05                       ; 0xc3f6f
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3f71
    jnbe short 03f8fh                         ; 77 19                       ; 0xc3f74
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3f76 vbe.c:536
    je short 03f8ch                           ; 74 11                       ; 0xc3f79
    mov ax, cx                                ; 89 c8                       ; 0xc3f7b vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f7d
    out DX, ax                                ; ef                          ; 0xc3f80
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f81 vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3f84
    mov es, si                                ; 8e c6                       ; 0xc3f85 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f87
    inc bx                                    ; 43                          ; 0xc3f8a vbe.c:539
    inc bx                                    ; 43                          ; 0xc3f8b
    inc cx                                    ; 41                          ; 0xc3f8c vbe.c:541
    jmp short 03f71h                          ; eb e2                       ; 0xc3f8d
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3f8f vbe.c:542
    pop si                                    ; 5e                          ; 0xc3f92
    pop cx                                    ; 59                          ; 0xc3f93
    pop bx                                    ; 5b                          ; 0xc3f94
    pop bp                                    ; 5d                          ; 0xc3f95
    retn                                      ; c3                          ; 0xc3f96
  ; disGetNextSymbol 0xc3f97 LB 0x2ec -> off=0x0 cb=000000000000008f uValue=00000000000c3f97 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3f97 LB 0x8f
    push bp                                   ; 55                          ; 0xc3f97 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3f98
    push bx                                   ; 53                          ; 0xc3f9a
    push cx                                   ; 51                          ; 0xc3f9b
    push si                                   ; 56                          ; 0xc3f9c
    push ax                                   ; 50                          ; 0xc3f9d
    mov cx, ax                                ; 89 c1                       ; 0xc3f9e
    mov bx, dx                                ; 89 d3                       ; 0xc3fa0
    mov es, ax                                ; 8e c0                       ; 0xc3fa2 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fa4
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3fa7
    inc bx                                    ; 43                          ; 0xc3faa vbe.c:550
    inc bx                                    ; 43                          ; 0xc3fab
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3fac vbe.c:552
    jne short 03fc2h                          ; 75 10                       ; 0xc3fb0
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3fb2 vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fb5
    out DX, ax                                ; ef                          ; 0xc3fb8
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3fb9 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fbc
    out DX, ax                                ; ef                          ; 0xc3fbf
    jmp short 0401eh                          ; eb 5c                       ; 0xc3fc0 vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3fc2 vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fc5
    out DX, ax                                ; ef                          ; 0xc3fc8
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fc9 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fcc vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fcf
    inc bx                                    ; 43                          ; 0xc3fd0 vbe.c:558
    inc bx                                    ; 43                          ; 0xc3fd1
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3fd2
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fd5
    out DX, ax                                ; ef                          ; 0xc3fd8
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fd9 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fdc vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fdf
    inc bx                                    ; 43                          ; 0xc3fe0 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3fe1
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3fe2
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fe5
    out DX, ax                                ; ef                          ; 0xc3fe8
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fe9 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fec vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fef
    inc bx                                    ; 43                          ; 0xc3ff0 vbe.c:564
    inc bx                                    ; 43                          ; 0xc3ff1
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3ff2
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ff5
    out DX, ax                                ; ef                          ; 0xc3ff8
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3ff9 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ffc
    out DX, ax                                ; ef                          ; 0xc3fff
    mov si, strict word 00005h                ; be 05 00                    ; 0xc4000 vbe.c:568
    jmp short 0400ah                          ; eb 05                       ; 0xc4003
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc4005
    jnbe short 0401eh                         ; 77 14                       ; 0xc4008
    mov ax, si                                ; 89 f0                       ; 0xc400a vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc400c
    out DX, ax                                ; ef                          ; 0xc400f
    mov es, cx                                ; 8e c1                       ; 0xc4010 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4012
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4015 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc4018
    inc bx                                    ; 43                          ; 0xc4019 vbe.c:571
    inc bx                                    ; 43                          ; 0xc401a
    inc si                                    ; 46                          ; 0xc401b vbe.c:572
    jmp short 04005h                          ; eb e7                       ; 0xc401c
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc401e vbe.c:574
    pop si                                    ; 5e                          ; 0xc4021
    pop cx                                    ; 59                          ; 0xc4022
    pop bx                                    ; 5b                          ; 0xc4023
    pop bp                                    ; 5d                          ; 0xc4024
    retn                                      ; c3                          ; 0xc4025
  ; disGetNextSymbol 0xc4026 LB 0x25d -> off=0x0 cb=000000000000008c uValue=00000000000c4026 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc4026 LB 0x8c
    push bp                                   ; 55                          ; 0xc4026 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc4027
    push si                                   ; 56                          ; 0xc4029
    push di                                   ; 57                          ; 0xc402a
    push ax                                   ; 50                          ; 0xc402b
    mov si, ax                                ; 89 c6                       ; 0xc402c
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc402e
    mov ax, bx                                ; 89 d8                       ; 0xc4031
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc4033
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc4036 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc4039 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc403b
    je short 04085h                           ; 74 45                       ; 0xc403e
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc4040
    je short 04069h                           ; 74 24                       ; 0xc4043
    test ax, ax                               ; 85 c0                       ; 0xc4045
    jne short 040a1h                          ; 75 58                       ; 0xc4047
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4049 vbe.c:598
    call 03004h                               ; e8 b5 ef                    ; 0xc404c
    mov cx, ax                                ; 89 c1                       ; 0xc404f
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4051 vbe.c:602
    je short 0405ch                           ; 74 05                       ; 0xc4055
    call 03f44h                               ; e8 ea fe                    ; 0xc4057 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc405a
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc405c vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc405f
    push SS                                   ; 16                          ; 0xc4062
    pop ES                                    ; 07                          ; 0xc4063
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4064
    jmp short 040a4h                          ; eb 3b                       ; 0xc4067 vbe.c:605
    push SS                                   ; 16                          ; 0xc4069 vbe.c:607
    pop ES                                    ; 07                          ; 0xc406a
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc406b
    mov dx, cx                                ; 89 ca                       ; 0xc406e vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4070
    call 0303fh                               ; e8 c9 ef                    ; 0xc4073
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4076 vbe.c:612
    je short 040a4h                           ; 74 28                       ; 0xc407a
    mov dx, ax                                ; 89 c2                       ; 0xc407c vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc407e
    call 03f4ch                               ; e8 c9 fe                    ; 0xc4080
    jmp short 040a4h                          ; eb 1f                       ; 0xc4083 vbe.c:614
    push SS                                   ; 16                          ; 0xc4085 vbe.c:616
    pop ES                                    ; 07                          ; 0xc4086
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4087
    mov dx, cx                                ; 89 ca                       ; 0xc408a vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc408c
    call 03317h                               ; e8 85 f2                    ; 0xc408f
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4092 vbe.c:621
    je short 040a4h                           ; 74 0c                       ; 0xc4096
    mov dx, ax                                ; 89 c2                       ; 0xc4098 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc409a
    call 03f97h                               ; e8 f8 fe                    ; 0xc409c
    jmp short 040a4h                          ; eb 03                       ; 0xc409f vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc40a1 vbe.c:626
    push SS                                   ; 16                          ; 0xc40a4 vbe.c:629
    pop ES                                    ; 07                          ; 0xc40a5
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc40a6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc40a9 vbe.c:630
    pop di                                    ; 5f                          ; 0xc40ac
    pop si                                    ; 5e                          ; 0xc40ad
    pop bp                                    ; 5d                          ; 0xc40ae
    retn 00002h                               ; c2 02 00                    ; 0xc40af
  ; disGetNextSymbol 0xc40b2 LB 0x1d1 -> off=0x0 cb=00000000000000df uValue=00000000000c40b2 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc40b2 LB 0xdf
    push bp                                   ; 55                          ; 0xc40b2 vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc40b3
    push si                                   ; 56                          ; 0xc40b5
    push di                                   ; 57                          ; 0xc40b6
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc40b7
    push ax                                   ; 50                          ; 0xc40ba
    mov di, dx                                ; 89 d7                       ; 0xc40bb
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc40bd
    mov si, cx                                ; 89 ce                       ; 0xc40c0
    call 03b55h                               ; e8 90 fa                    ; 0xc40c2 vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc40c5 vbe.c:661
    jne short 040ceh                          ; 75 05                       ; 0xc40c7
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc40c9
    jmp short 040d2h                          ; eb 04                       ; 0xc40cc
    xor ah, ah                                ; 30 e4                       ; 0xc40ce
    mov bx, ax                                ; 89 c3                       ; 0xc40d0
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc40d2
    call 03b8dh                               ; e8 b5 fa                    ; 0xc40d5 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc40d8
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc40db vbe.c:663
    push SS                                   ; 16                          ; 0xc40e0 vbe.c:664
    pop ES                                    ; 07                          ; 0xc40e1
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc40e2
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc40e5
    mov cl, byte [es:di]                      ; 26 8a 0d                    ; 0xc40e8 vbe.c:665
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc40eb vbe.c:669
    je short 040fch                           ; 74 0c                       ; 0xc40ee
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc40f0
    je short 04122h                           ; 74 2d                       ; 0xc40f3
    test cl, cl                               ; 84 c9                       ; 0xc40f5
    je short 0411dh                           ; 74 24                       ; 0xc40f7
    jmp near 0417ah                           ; e9 7e 00                    ; 0xc40f9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc40fc vbe.c:671
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc40ff
    jne short 04108h                          ; 75 05                       ; 0xc4101
    sal bx, 003h                              ; c1 e3 03                    ; 0xc4103 vbe.c:672
    jmp short 0411dh                          ; eb 15                       ; 0xc4106 vbe.c:673
    xor ah, ah                                ; 30 e4                       ; 0xc4108 vbe.c:674
    cwd                                       ; 99                          ; 0xc410a
    sal dx, 003h                              ; c1 e2 03                    ; 0xc410b
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc410e
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4110
    mov cx, ax                                ; 89 c1                       ; 0xc4113
    mov ax, bx                                ; 89 d8                       ; 0xc4115
    xor dx, dx                                ; 31 d2                       ; 0xc4117
    div cx                                    ; f7 f1                       ; 0xc4119
    mov bx, ax                                ; 89 c3                       ; 0xc411b
    mov ax, bx                                ; 89 d8                       ; 0xc411d vbe.c:677
    call 03b6eh                               ; e8 4c fa                    ; 0xc411f
    call 03b8dh                               ; e8 68 fa                    ; 0xc4122 vbe.c:680
    mov cx, ax                                ; 89 c1                       ; 0xc4125
    push SS                                   ; 16                          ; 0xc4127 vbe.c:681
    pop ES                                    ; 07                          ; 0xc4128
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc4129
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc412c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc412f vbe.c:682
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc4132
    jne short 0413dh                          ; 75 07                       ; 0xc4134
    mov bx, cx                                ; 89 cb                       ; 0xc4136 vbe.c:683
    shr bx, 003h                              ; c1 eb 03                    ; 0xc4138
    jmp short 04150h                          ; eb 13                       ; 0xc413b vbe.c:684
    xor ah, ah                                ; 30 e4                       ; 0xc413d vbe.c:685
    cwd                                       ; 99                          ; 0xc413f
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4140
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4143
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4145
    mov bx, ax                                ; 89 c3                       ; 0xc4148
    mov ax, cx                                ; 89 c8                       ; 0xc414a
    mul bx                                    ; f7 e3                       ; 0xc414c
    mov bx, ax                                ; 89 c3                       ; 0xc414e
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc4150 vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc4153
    push SS                                   ; 16                          ; 0xc4156 vbe.c:687
    pop ES                                    ; 07                          ; 0xc4157
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc4158
    call 03ba6h                               ; e8 48 fa                    ; 0xc415b vbe.c:688
    push SS                                   ; 16                          ; 0xc415e
    pop ES                                    ; 07                          ; 0xc415f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc4160
    call 03b1dh                               ; e8 b7 f9                    ; 0xc4163 vbe.c:689
    push SS                                   ; 16                          ; 0xc4166
    pop ES                                    ; 07                          ; 0xc4167
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc4168
    jbe short 0417fh                          ; 76 12                       ; 0xc416b
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc416d vbe.c:690
    call 03b6eh                               ; e8 fb f9                    ; 0xc4170
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc4173 vbe.c:691
    jmp short 0417fh                          ; eb 05                       ; 0xc4178 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc417a vbe.c:696
    push SS                                   ; 16                          ; 0xc417f vbe.c:699
    pop ES                                    ; 07                          ; 0xc4180
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc4181
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc4184
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4187
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc418a vbe.c:700
    pop di                                    ; 5f                          ; 0xc418d
    pop si                                    ; 5e                          ; 0xc418e
    pop bp                                    ; 5d                          ; 0xc418f
    retn                                      ; c3                          ; 0xc4190
  ; disGetNextSymbol 0xc4191 LB 0xf2 -> off=0x0 cb=00000000000000f2 uValue=00000000000c4191 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc4191 LB 0xf2
    push bp                                   ; 55                          ; 0xc4191 vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc4192
    push si                                   ; 56                          ; 0xc4194
    push di                                   ; 57                          ; 0xc4195
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc4196
    mov di, ax                                ; 89 c7                       ; 0xc4199
    mov si, dx                                ; 89 d6                       ; 0xc419b
    mov dx, cx                                ; 89 ca                       ; 0xc419d
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc419f vbe.c:739
    push SS                                   ; 16                          ; 0xc41a4 vbe.c:740
    pop ES                                    ; 07                          ; 0xc41a5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc41a6
    test al, al                               ; 84 c0                       ; 0xc41a9 vbe.c:741
    jne short 041cfh                          ; 75 22                       ; 0xc41ab
    push SS                                   ; 16                          ; 0xc41ad vbe.c:743
    pop ES                                    ; 07                          ; 0xc41ae
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc41af
    mov bx, dx                                ; 89 d3                       ; 0xc41b2 vbe.c:744
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc41b4
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc41b7 vbe.c:745
    shr ax, 008h                              ; c1 e8 08                    ; 0xc41ba
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc41bd
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc41c0
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc41c3 vbe.c:750
    je short 041d7h                           ; 74 10                       ; 0xc41c5
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc41c7
    je short 041d7h                           ; 74 0c                       ; 0xc41c9
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc41cb
    je short 041d7h                           ; 74 08                       ; 0xc41cd
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc41cf vbe.c:751
    jmp near 04274h                           ; e9 9d 00                    ; 0xc41d4 vbe.c:752
    push SS                                   ; 16                          ; 0xc41d7 vbe.c:756
    pop ES                                    ; 07                          ; 0xc41d8
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc41d9
    je short 041e5h                           ; 74 05                       ; 0xc41de
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41e0
    jmp short 041e7h                          ; eb 02                       ; 0xc41e3
    xor ax, ax                                ; 31 c0                       ; 0xc41e5
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc41e7
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc41ea vbe.c:759
    jnc short 041f5h                          ; 73 05                       ; 0xc41ee
    mov cx, 00280h                            ; b9 80 02                    ; 0xc41f0 vbe.c:760
    jmp short 041feh                          ; eb 09                       ; 0xc41f3 vbe.c:761
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc41f5
    jbe short 041feh                          ; 76 03                       ; 0xc41f9
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc41fb vbe.c:762
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc41fe vbe.c:763
    jnc short 04209h                          ; 73 05                       ; 0xc4202
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc4204 vbe.c:764
    jmp short 04212h                          ; eb 09                       ; 0xc4207 vbe.c:765
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc4209
    jbe short 04212h                          ; 76 03                       ; 0xc420d
    mov bx, 00780h                            ; bb 80 07                    ; 0xc420f vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4212 vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4215
    call 03bbfh                               ; e8 a4 f9                    ; 0xc4218
    mov si, ax                                ; 89 c6                       ; 0xc421b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc421d vbe.c:775
    xor ah, ah                                ; 30 e4                       ; 0xc4220
    cwd                                       ; 99                          ; 0xc4222
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4223
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4226
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4228
    mov dx, ax                                ; 89 c2                       ; 0xc422b
    mov ax, cx                                ; 89 c8                       ; 0xc422d
    mul dx                                    ; f7 e2                       ; 0xc422f
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc4231 vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc4234
    mov dx, bx                                ; 89 da                       ; 0xc4236 vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc4238
    cmp dx, si                                ; 39 f2                       ; 0xc423a vbe.c:780
    jnbe short 04244h                         ; 77 06                       ; 0xc423c
    jne short 0424bh                          ; 75 0b                       ; 0xc423e
    test ax, ax                               ; 85 c0                       ; 0xc4240
    jbe short 0424bh                          ; 76 07                       ; 0xc4242
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc4244 vbe.c:782
    jmp short 04274h                          ; eb 29                       ; 0xc4249 vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc424b vbe.c:787
    call 005ddh                               ; e8 8d c3                    ; 0xc424d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc4250 vbe.c:788
    xor ah, ah                                ; 30 e4                       ; 0xc4253
    call 03b36h                               ; e8 de f8                    ; 0xc4255
    mov ax, cx                                ; 89 c8                       ; 0xc4258 vbe.c:789
    call 03adfh                               ; e8 82 f8                    ; 0xc425a
    mov ax, bx                                ; 89 d8                       ; 0xc425d vbe.c:790
    call 03afeh                               ; e8 9c f8                    ; 0xc425f
    xor ax, ax                                ; 31 c0                       ; 0xc4262 vbe.c:791
    call 00603h                               ; e8 9c c3                    ; 0xc4264
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4267 vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc426a
    xor ah, ah                                ; 30 e4                       ; 0xc426c
    call 005ddh                               ; e8 6c c3                    ; 0xc426e
    call 006d2h                               ; e8 5e c4                    ; 0xc4271 vbe.c:793
    push SS                                   ; 16                          ; 0xc4274 vbe.c:801
    pop ES                                    ; 07                          ; 0xc4275
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4276
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc4279
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc427c vbe.c:802
    pop di                                    ; 5f                          ; 0xc427f
    pop si                                    ; 5e                          ; 0xc4280
    pop bp                                    ; 5d                          ; 0xc4281
    retn                                      ; c3                          ; 0xc4282

  ; Padding 0x37d bytes at 0xc4283
  times 893 db 0

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
    db  'Oracle VM VirtualBox Version 6.1.1 VGA BIOS', 00dh, 00ah, 000h
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
    db  'Oracle VM VirtualBox Version 6.1.1', 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 024h
