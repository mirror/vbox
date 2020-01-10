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





section VGAROM progbits vstart=0x0 align=1 ; size=0x90a class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x90a -> off=0x22 cb=000000000000054e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e2h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    call 008f3h                               ; e8 19 08                    ; 0xc00d7 vgarom.asm:189
    jmp short 000eah                          ; eb 0e                       ; 0xc00da vgarom.asm:190
    push ES                                   ; 06                          ; 0xc00dc vgarom.asm:194
    push DS                                   ; 1e                          ; 0xc00dd vgarom.asm:195
    pushaw                                    ; 60                          ; 0xc00de vgarom.asm:97
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc00df vgarom.asm:199
    mov ds, bx                                ; 8e db                       ; 0xc00e2 vgarom.asm:200
    call 0347ch                               ; e8 95 33                    ; 0xc00e4 vgarom.asm:201
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
  ; disGetNextSymbol 0xc0570 LB 0x39a -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x393 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05b7 LB 0x353 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc05dd LB 0x32d -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc0603 LB 0x307 -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc0629 LB 0x2e1 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06d2 LB 0x238 -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07bf LB 0x14b -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07d2 LB 0x138 -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc07f7 LB 0x113 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc0824 LB 0xe6 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0858 LB 0xb2 -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc088f LB 0x7b -> off=0x0 cb=0000000000000064 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x64
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008cah                           ; 74 32                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008efh                          ; 76 52                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008ebh                          ; 75 49                       ; 0xc08a0 vberom.asm:690
    pushad                                    ; 66 60                       ; 0xc08a2 vberom.asm:131
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
    popad                                     ; 66 61                       ; 0xc08c4 vberom.asm:150
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08c6 vberom.asm:727
    retn                                      ; c3                          ; 0xc08c9 vberom.asm:728
    pushad                                    ; 66 60                       ; 0xc08ca vberom.asm:131
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
    popad                                     ; 66 61                       ; 0xc08e7 vberom.asm:150
    jmp short 008c6h                          ; eb db                       ; 0xc08e9 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08eb vberom.asm:762
    retn                                      ; c3                          ; 0xc08ee vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08ef vberom.asm:765
    retn                                      ; c3                          ; 0xc08f2 vberom.asm:766
  ; disGetNextSymbol 0xc08f3 LB 0x17 -> off=0x0 cb=0000000000000017 uValue=00000000000c08f3 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08f3 LB 0x17
    test bl, bl                               ; 84 db                       ; 0xc08f3 vberom.asm:780
    jne short 00906h                          ; 75 0f                       ; 0xc08f5 vberom.asm:781
    mov di, 0c000h                            ; bf 00 c0                    ; 0xc08f7 vberom.asm:782
    mov es, di                                ; 8e c7                       ; 0xc08fa vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc08fc vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08ff vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0902 vberom.asm:786
    retn                                      ; c3                          ; 0xc0905 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0906 vberom.asm:789
    retn                                      ; c3                          ; 0xc0909 vberom.asm:790

  ; Padding 0x76 bytes at 0xc090a
  times 118 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x36a5 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x36a5 -> off=0x0 cb=000000000000001a uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1a
    push bx                                   ; 53                          ; 0xc0980 vgabios.c:87
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0984 vgabios.c:91
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0987
    xor ax, ax                                ; 31 c0                       ; 0xc098a
    mov es, ax                                ; 8e c0                       ; 0xc098c
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc098e
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0991
    pop bp                                    ; 5d                          ; 0xc0997 vgabios.c:92
    pop bx                                    ; 5b                          ; 0xc0998
    retn                                      ; c3                          ; 0xc0999
  ; disGetNextSymbol 0xc099a LB 0x368b -> off=0x0 cb=000000000000001c uValue=00000000000c099a 'init_vga_card'
init_vga_card:                               ; 0xc099a LB 0x1c
    push bp                                   ; 55                          ; 0xc099a vgabios.c:143
    mov bp, sp                                ; 89 e5                       ; 0xc099b
    push dx                                   ; 52                          ; 0xc099d
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc099e vgabios.c:146
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a0
    out DX, AL                                ; ee                          ; 0xc09a3
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a4 vgabios.c:149
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a6
    out DX, AL                                ; ee                          ; 0xc09a9
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09aa vgabios.c:150
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ac
    out DX, AL                                ; ee                          ; 0xc09af
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b0 vgabios.c:155
    pop dx                                    ; 5a                          ; 0xc09b3
    pop bp                                    ; 5d                          ; 0xc09b4
    retn                                      ; c3                          ; 0xc09b5
  ; disGetNextSymbol 0xc09b6 LB 0x366f -> off=0x0 cb=0000000000000032 uValue=00000000000c09b6 'init_bios_area'
init_bios_area:                              ; 0xc09b6 LB 0x32
    push bx                                   ; 53                          ; 0xc09b6 vgabios.c:164
    push bp                                   ; 55                          ; 0xc09b7
    mov bp, sp                                ; 89 e5                       ; 0xc09b8
    xor bx, bx                                ; 31 db                       ; 0xc09ba vgabios.c:168
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09bc
    mov es, ax                                ; 8e c0                       ; 0xc09bf
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c1 vgabios.c:171
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c5
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c7
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09c9
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09cd vgabios.c:175
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d3 vgabios.c:177
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09da vgabios.c:181
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e0 vgabios.c:183
    pop bp                                    ; 5d                          ; 0xc09e5 vgabios.c:184
    pop bx                                    ; 5b                          ; 0xc09e6
    retn                                      ; c3                          ; 0xc09e7
  ; disGetNextSymbol 0xc09e8 LB 0x363d -> off=0x0 cb=0000000000000020 uValue=00000000000c09e8 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09e8 LB 0x20
    push bp                                   ; 55                          ; 0xc09e8 vgabios.c:224
    mov bp, sp                                ; 89 e5                       ; 0xc09e9
    call 0099ah                               ; e8 ac ff                    ; 0xc09eb vgabios.c:226
    call 009b6h                               ; e8 c5 ff                    ; 0xc09ee vgabios.c:227
    call 039deh                               ; e8 ea 2f                    ; 0xc09f1 vgabios.c:229
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc09f4 vgabios.c:231
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09f7
    call 00980h                               ; e8 83 ff                    ; 0xc09fa
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc09fd vgabios.c:257
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a00
    int 010h                                  ; cd 10                       ; 0xc0a02
    mov sp, bp                                ; 89 ec                       ; 0xc0a04 vgabios.c:260
    pop bp                                    ; 5d                          ; 0xc0a06
    retf                                      ; cb                          ; 0xc0a07
  ; disGetNextSymbol 0xc0a08 LB 0x361d -> off=0x0 cb=000000000000003f uValue=00000000000c0a08 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a08 LB 0x3f
    push si                                   ; 56                          ; 0xc0a08 vgabios.c:329
    push di                                   ; 57                          ; 0xc0a09
    push bp                                   ; 55                          ; 0xc0a0a
    mov bp, sp                                ; 89 e5                       ; 0xc0a0b
    mov si, dx                                ; 89 d6                       ; 0xc0a0d
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a0f vgabios.c:331
    jbe short 00a21h                          ; 76 0e                       ; 0xc0a11
    push SS                                   ; 16                          ; 0xc0a13 vgabios.c:332
    pop ES                                    ; 07                          ; 0xc0a14
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a15
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a1a vgabios.c:333
    jmp short 00a43h                          ; eb 22                       ; 0xc0a1f vgabios.c:334
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a21 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a24
    mov es, dx                                ; 8e c2                       ; 0xc0a27
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a29
    push SS                                   ; 16                          ; 0xc0a2c vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a2d
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a2e
    movzx si, al                              ; 0f b6 f0                    ; 0xc0a31 vgabios.c:337
    add si, si                                ; 01 f6                       ; 0xc0a34
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a36
    mov es, dx                                ; 8e c2                       ; 0xc0a39 vgabios.c:47
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0a3b
    push SS                                   ; 16                          ; 0xc0a3e vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a3f
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc0a40
    pop bp                                    ; 5d                          ; 0xc0a43 vgabios.c:339
    pop di                                    ; 5f                          ; 0xc0a44
    pop si                                    ; 5e                          ; 0xc0a45
    retn                                      ; c3                          ; 0xc0a46
  ; disGetNextSymbol 0xc0a47 LB 0x35de -> off=0x0 cb=000000000000005d uValue=00000000000c0a47 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a47 LB 0x5d
    push bp                                   ; 55                          ; 0xc0a47 vgabios.c:342
    mov bp, sp                                ; 89 e5                       ; 0xc0a48
    push si                                   ; 56                          ; 0xc0a4a
    push di                                   ; 57                          ; 0xc0a4b
    push ax                                   ; 50                          ; 0xc0a4c
    push ax                                   ; 50                          ; 0xc0a4d
    push dx                                   ; 52                          ; 0xc0a4e
    push bx                                   ; 53                          ; 0xc0a4f
    mov bl, cl                                ; 88 cb                       ; 0xc0a50
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a52 vgabios.c:344
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a57 vgabios.c:346
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a5a
    je short 00a98h                           ; 74 38                       ; 0xc0a5e
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc0a60 vgabios.c:347
    mov dx, ss                                ; 8c d2                       ; 0xc0a64
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a66
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a69
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a6c
    push DS                                   ; 1e                          ; 0xc0a6f
    mov ds, dx                                ; 8e da                       ; 0xc0a70
    rep cmpsb                                 ; f3 a6                       ; 0xc0a72
    pop DS                                    ; 1f                          ; 0xc0a74
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a75
    je near 00a7eh                            ; 0f 84 02 00                 ; 0xc0a78
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a7c
    test ax, ax                               ; 85 c0                       ; 0xc0a7e
    jne short 00a8dh                          ; 75 0b                       ; 0xc0a80
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc0a82 vgabios.c:348
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a85
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a88
    jmp short 00a98h                          ; eb 0b                       ; 0xc0a8b vgabios.c:349
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc0a8d vgabios.c:351
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0a91
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0a94 vgabios.c:352
    jmp short 00a57h                          ; eb bf                       ; 0xc0a96 vgabios.c:353
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0a98 vgabios.c:355
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0a9b
    pop di                                    ; 5f                          ; 0xc0a9e
    pop si                                    ; 5e                          ; 0xc0a9f
    pop bp                                    ; 5d                          ; 0xc0aa0
    retn 00004h                               ; c2 04 00                    ; 0xc0aa1
  ; disGetNextSymbol 0xc0aa4 LB 0x3581 -> off=0x0 cb=0000000000000046 uValue=00000000000c0aa4 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0aa4 LB 0x46
    push bp                                   ; 55                          ; 0xc0aa4 vgabios.c:357
    mov bp, sp                                ; 89 e5                       ; 0xc0aa5
    push si                                   ; 56                          ; 0xc0aa7
    push di                                   ; 57                          ; 0xc0aa8
    push ax                                   ; 50                          ; 0xc0aa9
    push ax                                   ; 50                          ; 0xc0aaa
    mov si, ax                                ; 89 c6                       ; 0xc0aab
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0aad
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0ab0
    mov bx, cx                                ; 89 cb                       ; 0xc0ab3
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0ab5 vgabios.c:364
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0ab8
    out DX, ax                                ; ef                          ; 0xc0abb
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0abc vgabios.c:366
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0abf
    je short 00adah                           ; 74 15                       ; 0xc0ac3
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0ac5 vgabios.c:367
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0ac8
    not al                                    ; f6 d0                       ; 0xc0acb
    mov di, bx                                ; 89 df                       ; 0xc0acd
    inc bx                                    ; 43                          ; 0xc0acf
    push SS                                   ; 16                          ; 0xc0ad0
    pop ES                                    ; 07                          ; 0xc0ad1
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ad2
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0ad5 vgabios.c:368
    jmp short 00abch                          ; eb e2                       ; 0xc0ad8 vgabios.c:369
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0ada vgabios.c:372
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0add
    out DX, ax                                ; ef                          ; 0xc0ae0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ae1 vgabios.c:373
    pop di                                    ; 5f                          ; 0xc0ae4
    pop si                                    ; 5e                          ; 0xc0ae5
    pop bp                                    ; 5d                          ; 0xc0ae6
    retn 00002h                               ; c2 02 00                    ; 0xc0ae7
  ; disGetNextSymbol 0xc0aea LB 0x353b -> off=0x0 cb=000000000000002a uValue=00000000000c0aea 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0aea LB 0x2a
    push bp                                   ; 55                          ; 0xc0aea vgabios.c:375
    mov bp, sp                                ; 89 e5                       ; 0xc0aeb
    xor dh, dh                                ; 30 f6                       ; 0xc0aed vgabios.c:379
    imul bx, dx                               ; 0f af da                    ; 0xc0aef
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc0af2
    imul bx, dx                               ; 0f af da                    ; 0xc0af6
    xor ah, ah                                ; 30 e4                       ; 0xc0af9
    add ax, bx                                ; 01 d8                       ; 0xc0afb
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc0afd vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0b00
    mov es, dx                                ; 8e c2                       ; 0xc0b03
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0b05
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc0b08 vgabios.c:48
    imul dx, bx                               ; 0f af d3                    ; 0xc0b0b
    add ax, dx                                ; 01 d0                       ; 0xc0b0e
    pop bp                                    ; 5d                          ; 0xc0b10 vgabios.c:383
    retn 00002h                               ; c2 02 00                    ; 0xc0b11
  ; disGetNextSymbol 0xc0b14 LB 0x3511 -> off=0x0 cb=000000000000003e uValue=00000000000c0b14 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b14 LB 0x3e
    push bp                                   ; 55                          ; 0xc0b14 vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b15
    push cx                                   ; 51                          ; 0xc0b17
    push si                                   ; 56                          ; 0xc0b18
    push di                                   ; 57                          ; 0xc0b19
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0b1a
    mov si, ax                                ; 89 c6                       ; 0xc0b1d
    mov ax, dx                                ; 89 d0                       ; 0xc0b1f
    movzx di, bl                              ; 0f b6 fb                    ; 0xc0b21 vgabios.c:389
    push di                                   ; 57                          ; 0xc0b24
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0b25
    mov bx, si                                ; 89 f3                       ; 0xc0b28
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b2a
    call 00aa4h                               ; e8 74 ff                    ; 0xc0b2d
    push di                                   ; 57                          ; 0xc0b30 vgabios.c:392
    push 00100h                               ; 68 00 01                    ; 0xc0b31
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b34 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b37
    mov es, ax                                ; 8e c0                       ; 0xc0b39
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b3b
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b3e
    xor cx, cx                                ; 31 c9                       ; 0xc0b42 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0b44
    call 00a47h                               ; e8 fd fe                    ; 0xc0b47
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0b4a vgabios.c:393
    pop di                                    ; 5f                          ; 0xc0b4d
    pop si                                    ; 5e                          ; 0xc0b4e
    pop cx                                    ; 59                          ; 0xc0b4f
    pop bp                                    ; 5d                          ; 0xc0b50
    retn                                      ; c3                          ; 0xc0b51
  ; disGetNextSymbol 0xc0b52 LB 0x34d3 -> off=0x0 cb=000000000000001a uValue=00000000000c0b52 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b52 LB 0x1a
    push bp                                   ; 55                          ; 0xc0b52 vgabios.c:395
    mov bp, sp                                ; 89 e5                       ; 0xc0b53
    xor dh, dh                                ; 30 f6                       ; 0xc0b55 vgabios.c:399
    imul dx, bx                               ; 0f af d3                    ; 0xc0b57
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc0b5a
    imul bx, dx                               ; 0f af da                    ; 0xc0b5e
    xor ah, ah                                ; 30 e4                       ; 0xc0b61
    add ax, bx                                ; 01 d8                       ; 0xc0b63
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0b65 vgabios.c:400
    pop bp                                    ; 5d                          ; 0xc0b68 vgabios.c:402
    retn 00002h                               ; c2 02 00                    ; 0xc0b69
  ; disGetNextSymbol 0xc0b6c LB 0x34b9 -> off=0x0 cb=000000000000004b uValue=00000000000c0b6c 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b6c LB 0x4b
    push si                                   ; 56                          ; 0xc0b6c vgabios.c:404
    push di                                   ; 57                          ; 0xc0b6d
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0b6e
    mov si, ax                                ; 89 c6                       ; 0xc0b72
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0b74
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0b77
    mov bx, cx                                ; 89 cb                       ; 0xc0b7a
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0b7c vgabios.c:410
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0b7f
    je short 00bb1h                           ; 74 2c                       ; 0xc0b83
    xor dh, dh                                ; 30 f6                       ; 0xc0b85 vgabios.c:411
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0b87 vgabios.c:412
    xor ax, ax                                ; 31 c0                       ; 0xc0b89 vgabios.c:413
    jmp short 00b92h                          ; eb 05                       ; 0xc0b8b
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0b8d
    jnl short 00ba6h                          ; 7d 14                       ; 0xc0b90
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0b92 vgabios.c:414
    mov di, si                                ; 89 f7                       ; 0xc0b95
    add di, ax                                ; 01 c7                       ; 0xc0b97
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0b99
    je short 00ba1h                           ; 74 02                       ; 0xc0b9d
    or dh, dl                                 ; 08 d6                       ; 0xc0b9f vgabios.c:415
    shr dl, 1                                 ; d0 ea                       ; 0xc0ba1 vgabios.c:416
    inc ax                                    ; 40                          ; 0xc0ba3 vgabios.c:417
    jmp short 00b8dh                          ; eb e7                       ; 0xc0ba4
    mov di, bx                                ; 89 df                       ; 0xc0ba6 vgabios.c:418
    inc bx                                    ; 43                          ; 0xc0ba8
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0ba9
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bac vgabios.c:419
    jmp short 00b7ch                          ; eb cb                       ; 0xc0baf vgabios.c:420
    leave                                     ; c9                          ; 0xc0bb1 vgabios.c:421
    pop di                                    ; 5f                          ; 0xc0bb2
    pop si                                    ; 5e                          ; 0xc0bb3
    retn 00002h                               ; c2 02 00                    ; 0xc0bb4
  ; disGetNextSymbol 0xc0bb7 LB 0x346e -> off=0x0 cb=000000000000003f uValue=00000000000c0bb7 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0bb7 LB 0x3f
    push bp                                   ; 55                          ; 0xc0bb7 vgabios.c:423
    mov bp, sp                                ; 89 e5                       ; 0xc0bb8
    push cx                                   ; 51                          ; 0xc0bba
    push si                                   ; 56                          ; 0xc0bbb
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0bbc
    mov cx, ax                                ; 89 c1                       ; 0xc0bbf
    mov ax, dx                                ; 89 d0                       ; 0xc0bc1
    movzx si, bl                              ; 0f b6 f3                    ; 0xc0bc3 vgabios.c:427
    push si                                   ; 56                          ; 0xc0bc6
    mov bx, cx                                ; 89 cb                       ; 0xc0bc7
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0bc9
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0bcc
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bcf
    call 00b6ch                               ; e8 97 ff                    ; 0xc0bd2
    push si                                   ; 56                          ; 0xc0bd5 vgabios.c:430
    push 00100h                               ; 68 00 01                    ; 0xc0bd6
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0bd9 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0bdc
    mov es, ax                                ; 8e c0                       ; 0xc0bde
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0be0
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0be3
    xor cx, cx                                ; 31 c9                       ; 0xc0be7 vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0be9
    call 00a47h                               ; e8 58 fe                    ; 0xc0bec
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0bef vgabios.c:431
    pop si                                    ; 5e                          ; 0xc0bf2
    pop cx                                    ; 59                          ; 0xc0bf3
    pop bp                                    ; 5d                          ; 0xc0bf4
    retn                                      ; c3                          ; 0xc0bf5
  ; disGetNextSymbol 0xc0bf6 LB 0x342f -> off=0x0 cb=0000000000000035 uValue=00000000000c0bf6 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0bf6 LB 0x35
    push bp                                   ; 55                          ; 0xc0bf6 vgabios.c:433
    mov bp, sp                                ; 89 e5                       ; 0xc0bf7
    push bx                                   ; 53                          ; 0xc0bf9
    push cx                                   ; 51                          ; 0xc0bfa
    mov bx, ax                                ; 89 c3                       ; 0xc0bfb
    mov es, dx                                ; 8e c2                       ; 0xc0bfd
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0bff vgabios.c:439
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c02 vgabios.c:440
    xor dl, dl                                ; 30 d2                       ; 0xc0c04 vgabios.c:441
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c06 vgabios.c:442
    xchg ah, al                               ; 86 c4                       ; 0xc0c09
    xor bx, bx                                ; 31 db                       ; 0xc0c0b vgabios.c:444
    jmp short 00c14h                          ; eb 05                       ; 0xc0c0d
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c0f
    jnl short 00c22h                          ; 7d 0e                       ; 0xc0c12
    test ax, cx                               ; 85 c8                       ; 0xc0c14 vgabios.c:445
    je short 00c1ah                           ; 74 02                       ; 0xc0c16
    or dl, dh                                 ; 08 f2                       ; 0xc0c18 vgabios.c:446
    shr dh, 1                                 ; d0 ee                       ; 0xc0c1a vgabios.c:447
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0c1c vgabios.c:448
    inc bx                                    ; 43                          ; 0xc0c1f vgabios.c:449
    jmp short 00c0fh                          ; eb ed                       ; 0xc0c20
    mov al, dl                                ; 88 d0                       ; 0xc0c22 vgabios.c:451
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c24
    pop cx                                    ; 59                          ; 0xc0c27
    pop bx                                    ; 5b                          ; 0xc0c28
    pop bp                                    ; 5d                          ; 0xc0c29
    retn                                      ; c3                          ; 0xc0c2a
  ; disGetNextSymbol 0xc0c2b LB 0x33fa -> off=0x0 cb=0000000000000084 uValue=00000000000c0c2b 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c2b LB 0x84
    push bp                                   ; 55                          ; 0xc0c2b vgabios.c:453
    mov bp, sp                                ; 89 e5                       ; 0xc0c2c
    push cx                                   ; 51                          ; 0xc0c2e
    push si                                   ; 56                          ; 0xc0c2f
    push di                                   ; 57                          ; 0xc0c30
    push ax                                   ; 50                          ; 0xc0c31
    mov si, dx                                ; 89 d6                       ; 0xc0c32
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c34 vgabios.c:461
    je short 00c73h                           ; 74 3a                       ; 0xc0c37
    mov bx, ax                                ; 89 c3                       ; 0xc0c39 vgabios.c:463
    add bx, ax                                ; 01 c3                       ; 0xc0c3b
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c3d
    xor cx, cx                                ; 31 c9                       ; 0xc0c42 vgabios.c:465
    jmp short 00c4bh                          ; eb 05                       ; 0xc0c44
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c46
    jnl short 00ca7h                          ; 7d 5c                       ; 0xc0c49
    mov ax, bx                                ; 89 d8                       ; 0xc0c4b vgabios.c:466
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c4d
    call 00bf6h                               ; e8 a3 ff                    ; 0xc0c50
    mov di, si                                ; 89 f7                       ; 0xc0c53
    inc si                                    ; 46                          ; 0xc0c55
    push SS                                   ; 16                          ; 0xc0c56
    pop ES                                    ; 07                          ; 0xc0c57
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c58
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c5b vgabios.c:467
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c5f
    call 00bf6h                               ; e8 91 ff                    ; 0xc0c62
    mov di, si                                ; 89 f7                       ; 0xc0c65
    inc si                                    ; 46                          ; 0xc0c67
    push SS                                   ; 16                          ; 0xc0c68
    pop ES                                    ; 07                          ; 0xc0c69
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c6a
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0c6d vgabios.c:468
    inc cx                                    ; 41                          ; 0xc0c70 vgabios.c:469
    jmp short 00c46h                          ; eb d3                       ; 0xc0c71
    mov bx, ax                                ; 89 c3                       ; 0xc0c73 vgabios.c:471
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c75
    xor cx, cx                                ; 31 c9                       ; 0xc0c7a vgabios.c:472
    jmp short 00c83h                          ; eb 05                       ; 0xc0c7c
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c7e
    jnl short 00ca7h                          ; 7d 24                       ; 0xc0c81
    mov di, si                                ; 89 f7                       ; 0xc0c83 vgabios.c:473
    inc si                                    ; 46                          ; 0xc0c85
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0c86
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0c89
    push SS                                   ; 16                          ; 0xc0c8c
    pop ES                                    ; 07                          ; 0xc0c8d
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c8e
    mov di, si                                ; 89 f7                       ; 0xc0c91 vgabios.c:474
    inc si                                    ; 46                          ; 0xc0c93
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0c94
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0c97
    push SS                                   ; 16                          ; 0xc0c9c
    pop ES                                    ; 07                          ; 0xc0c9d
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c9e
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0ca1 vgabios.c:475
    inc cx                                    ; 41                          ; 0xc0ca4 vgabios.c:476
    jmp short 00c7eh                          ; eb d7                       ; 0xc0ca5
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0ca7 vgabios.c:478
    pop di                                    ; 5f                          ; 0xc0caa
    pop si                                    ; 5e                          ; 0xc0cab
    pop cx                                    ; 59                          ; 0xc0cac
    pop bp                                    ; 5d                          ; 0xc0cad
    retn                                      ; c3                          ; 0xc0cae
  ; disGetNextSymbol 0xc0caf LB 0x3376 -> off=0x0 cb=0000000000000011 uValue=00000000000c0caf 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0caf LB 0x11
    push bp                                   ; 55                          ; 0xc0caf vgabios.c:480
    mov bp, sp                                ; 89 e5                       ; 0xc0cb0
    xor dh, dh                                ; 30 f6                       ; 0xc0cb2 vgabios.c:485
    imul dx, bx                               ; 0f af d3                    ; 0xc0cb4
    sal dx, 002h                              ; c1 e2 02                    ; 0xc0cb7
    xor ah, ah                                ; 30 e4                       ; 0xc0cba
    add ax, dx                                ; 01 d0                       ; 0xc0cbc
    pop bp                                    ; 5d                          ; 0xc0cbe vgabios.c:486
    retn                                      ; c3                          ; 0xc0cbf
  ; disGetNextSymbol 0xc0cc0 LB 0x3365 -> off=0x0 cb=0000000000000065 uValue=00000000000c0cc0 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0cc0 LB 0x65
    push bp                                   ; 55                          ; 0xc0cc0 vgabios.c:488
    mov bp, sp                                ; 89 e5                       ; 0xc0cc1
    push bx                                   ; 53                          ; 0xc0cc3
    push cx                                   ; 51                          ; 0xc0cc4
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0cc5
    movzx bx, dl                              ; 0f b6 da                    ; 0xc0cc8 vgabios.c:494
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0ccb
    call 00c2bh                               ; e8 5a ff                    ; 0xc0cce
    push strict byte 00008h                   ; 6a 08                       ; 0xc0cd1 vgabios.c:497
    push 00080h                               ; 68 80 00                    ; 0xc0cd3
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0cd6 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0cd9
    mov es, ax                                ; 8e c0                       ; 0xc0cdb
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0cdd
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0ce0
    xor cx, cx                                ; 31 c9                       ; 0xc0ce4 vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0ce6
    call 00a47h                               ; e8 5b fd                    ; 0xc0ce9
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0cec
    test ah, 080h                             ; f6 c4 80                    ; 0xc0cef vgabios.c:499
    jne short 00d1bh                          ; 75 27                       ; 0xc0cf2
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0cf4 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0cf7
    mov es, ax                                ; 8e c0                       ; 0xc0cf9
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0cfb
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0cfe
    test dx, dx                               ; 85 d2                       ; 0xc0d02 vgabios.c:503
    jne short 00d0ah                          ; 75 04                       ; 0xc0d04
    test ax, ax                               ; 85 c0                       ; 0xc0d06
    je short 00d1bh                           ; 74 11                       ; 0xc0d08
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d0a vgabios.c:504
    push 00080h                               ; 68 80 00                    ; 0xc0d0c
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0d0f
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d12
    call 00a47h                               ; e8 2f fd                    ; 0xc0d15
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d18
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d1b vgabios.c:507
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d1e
    pop cx                                    ; 59                          ; 0xc0d21
    pop bx                                    ; 5b                          ; 0xc0d22
    pop bp                                    ; 5d                          ; 0xc0d23
    retn                                      ; c3                          ; 0xc0d24
  ; disGetNextSymbol 0xc0d25 LB 0x3300 -> off=0x0 cb=0000000000000127 uValue=00000000000c0d25 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d25 LB 0x127
    push bp                                   ; 55                          ; 0xc0d25 vgabios.c:509
    mov bp, sp                                ; 89 e5                       ; 0xc0d26
    push bx                                   ; 53                          ; 0xc0d28
    push cx                                   ; 51                          ; 0xc0d29
    push si                                   ; 56                          ; 0xc0d2a
    push di                                   ; 57                          ; 0xc0d2b
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0d2c
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d2f
    mov si, dx                                ; 89 d6                       ; 0xc0d32
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d34 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d37
    mov es, ax                                ; 8e c0                       ; 0xc0d3a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d3c
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0d3f vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d42 vgabios.c:517
    call 033c0h                               ; e8 79 26                    ; 0xc0d44
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0d47
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d4a vgabios.c:518
    je near 00e43h                            ; 0f 84 f3 00                 ; 0xc0d4c
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc0d50 vgabios.c:522
    lea bx, [bp-018h]                         ; 8d 5e e8                    ; 0xc0d54
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0d57
    mov ax, cx                                ; 89 c8                       ; 0xc0d5a
    call 00a08h                               ; e8 a9 fc                    ; 0xc0d5c
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc0d5f vgabios.c:523
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0d62
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc0d65 vgabios.c:524
    xor al, al                                ; 30 c0                       ; 0xc0d68
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0d6a
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d6d
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0d70 vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0d73
    mov es, dx                                ; 8e c2                       ; 0xc0d76
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc0d78
    xor dh, dh                                ; 30 f6                       ; 0xc0d7b vgabios.c:38
    inc dx                                    ; 42                          ; 0xc0d7d
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0d7e vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0d81
    mov word [bp-014h], di                    ; 89 7e ec                    ; 0xc0d84 vgabios.c:48
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc0d87 vgabios.c:530
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0d8b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0d8e
    jne short 00dcbh                          ; 75 36                       ; 0xc0d93
    imul dx, di                               ; 0f af d7                    ; 0xc0d95 vgabios.c:532
    add dx, dx                                ; 01 d2                       ; 0xc0d98
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0d9a
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0d9d
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc0da0
    mov cx, word [bp-016h]                    ; 8b 4e ea                    ; 0xc0da4
    inc cx                                    ; 41                          ; 0xc0da7
    imul dx, cx                               ; 0f af d1                    ; 0xc0da8
    xor ah, ah                                ; 30 e4                       ; 0xc0dab
    imul di, ax                               ; 0f af f8                    ; 0xc0dad
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0db0
    add ax, di                                ; 01 f8                       ; 0xc0db4
    add ax, ax                                ; 01 c0                       ; 0xc0db6
    mov di, dx                                ; 89 d7                       ; 0xc0db8
    add di, ax                                ; 01 c7                       ; 0xc0dba
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0dbc vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0dc0
    push SS                                   ; 16                          ; 0xc0dc3 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0dc4
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0dc5
    jmp near 00e43h                           ; e9 78 00                    ; 0xc0dc8 vgabios.c:534
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0dcb vgabios.c:535
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0dcf
    je short 00e1fh                           ; 74 4b                       ; 0xc0dd2
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0dd4
    jc short 00e43h                           ; 72 6a                       ; 0xc0dd7
    jbe short 00de2h                          ; 76 07                       ; 0xc0dd9
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0ddb
    jbe short 00dfbh                          ; 76 1b                       ; 0xc0dde
    jmp short 00e43h                          ; eb 61                       ; 0xc0de0
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc0de2 vgabios.c:538
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0de6
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc0dea
    call 00cafh                               ; e8 bf fe                    ; 0xc0ded
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc0df0 vgabios.c:539
    call 00cc0h                               ; e8 c9 fe                    ; 0xc0df4
    xor ah, ah                                ; 30 e4                       ; 0xc0df7
    jmp short 00dc3h                          ; eb c8                       ; 0xc0df9
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0dfb vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0dfe
    xor dh, dh                                ; 30 f6                       ; 0xc0e01 vgabios.c:544
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e03
    push dx                                   ; 52                          ; 0xc0e06
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e07
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e0a
    mov bx, di                                ; 89 fb                       ; 0xc0e0e
    call 00aeah                               ; e8 d7 fc                    ; 0xc0e10
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e13 vgabios.c:545
    mov dx, ax                                ; 89 c2                       ; 0xc0e16
    mov ax, di                                ; 89 f8                       ; 0xc0e18
    call 00b14h                               ; e8 f7 fc                    ; 0xc0e1a
    jmp short 00df7h                          ; eb d8                       ; 0xc0e1d
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e1f vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0e22
    xor dh, dh                                ; 30 f6                       ; 0xc0e25 vgabios.c:549
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e27
    push dx                                   ; 52                          ; 0xc0e2a
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e2b
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e2e
    mov bx, di                                ; 89 fb                       ; 0xc0e32
    call 00b52h                               ; e8 1b fd                    ; 0xc0e34
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e37 vgabios.c:550
    mov dx, ax                                ; 89 c2                       ; 0xc0e3a
    mov ax, di                                ; 89 f8                       ; 0xc0e3c
    call 00bb7h                               ; e8 76 fd                    ; 0xc0e3e
    jmp short 00df7h                          ; eb b4                       ; 0xc0e41
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e43 vgabios.c:559
    pop di                                    ; 5f                          ; 0xc0e46
    pop si                                    ; 5e                          ; 0xc0e47
    pop cx                                    ; 59                          ; 0xc0e48
    pop bx                                    ; 5b                          ; 0xc0e49
    pop bp                                    ; 5d                          ; 0xc0e4a
    retn                                      ; c3                          ; 0xc0e4b
  ; disGetNextSymbol 0xc0e4c LB 0x31d9 -> off=0x10 cb=0000000000000084 uValue=00000000000c0e5c 'vga_get_font_info'
    db  073h, 00eh, 0b8h, 00eh, 0bdh, 00eh, 0c5h, 00eh, 0cah, 00eh, 0cfh, 00eh, 0d4h, 00eh, 0d9h, 00eh
vga_get_font_info:                           ; 0xc0e5c LB 0x84
    push si                                   ; 56                          ; 0xc0e5c vgabios.c:561
    push di                                   ; 57                          ; 0xc0e5d
    push bp                                   ; 55                          ; 0xc0e5e
    mov bp, sp                                ; 89 e5                       ; 0xc0e5f
    mov di, dx                                ; 89 d7                       ; 0xc0e61
    mov si, bx                                ; 89 de                       ; 0xc0e63
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0e65 vgabios.c:566
    jnbe short 00eb2h                         ; 77 48                       ; 0xc0e68
    mov bx, ax                                ; 89 c3                       ; 0xc0e6a
    add bx, ax                                ; 01 c3                       ; 0xc0e6c
    jmp word [cs:bx+00e4ch]                   ; 2e ff a7 4c 0e              ; 0xc0e6e
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0e73 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0e76
    mov es, ax                                ; 8e c0                       ; 0xc0e78
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e7a
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0e7d
    push SS                                   ; 16                          ; 0xc0e81 vgabios.c:569
    pop ES                                    ; 07                          ; 0xc0e82
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e83
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0e86
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e89
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e8c
    mov es, ax                                ; 8e c0                       ; 0xc0e8f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0e91
    xor ah, ah                                ; 30 e4                       ; 0xc0e94
    push SS                                   ; 16                          ; 0xc0e96
    pop ES                                    ; 07                          ; 0xc0e97
    mov bx, cx                                ; 89 cb                       ; 0xc0e98
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0e9a
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0e9d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ea0
    mov es, ax                                ; 8e c0                       ; 0xc0ea3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ea5
    xor ah, ah                                ; 30 e4                       ; 0xc0ea8
    push SS                                   ; 16                          ; 0xc0eaa
    pop ES                                    ; 07                          ; 0xc0eab
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0eac
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0eaf
    pop bp                                    ; 5d                          ; 0xc0eb2
    pop di                                    ; 5f                          ; 0xc0eb3
    pop si                                    ; 5e                          ; 0xc0eb4
    retn 00002h                               ; c2 02 00                    ; 0xc0eb5
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0eb8 vgabios.c:57
    jmp short 00e76h                          ; eb b9                       ; 0xc0ebb
    mov ax, 05d6ch                            ; b8 6c 5d                    ; 0xc0ebd vgabios.c:574
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc0ec0
    jmp short 00e81h                          ; eb bc                       ; 0xc0ec3 vgabios.c:575
    mov ax, 0556ch                            ; b8 6c 55                    ; 0xc0ec5 vgabios.c:577
    jmp short 00ec0h                          ; eb f6                       ; 0xc0ec8
    mov ax, 0596ch                            ; b8 6c 59                    ; 0xc0eca vgabios.c:580
    jmp short 00ec0h                          ; eb f1                       ; 0xc0ecd
    mov ax, 07b6ch                            ; b8 6c 7b                    ; 0xc0ecf vgabios.c:583
    jmp short 00ec0h                          ; eb ec                       ; 0xc0ed2
    mov ax, 06b6ch                            ; b8 6c 6b                    ; 0xc0ed4 vgabios.c:586
    jmp short 00ec0h                          ; eb e7                       ; 0xc0ed7
    mov ax, 07c99h                            ; b8 99 7c                    ; 0xc0ed9 vgabios.c:589
    jmp short 00ec0h                          ; eb e2                       ; 0xc0edc
    jmp short 00eb2h                          ; eb d2                       ; 0xc0ede vgabios.c:595
  ; disGetNextSymbol 0xc0ee0 LB 0x3145 -> off=0x0 cb=0000000000000156 uValue=00000000000c0ee0 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0ee0 LB 0x156
    push bp                                   ; 55                          ; 0xc0ee0 vgabios.c:608
    mov bp, sp                                ; 89 e5                       ; 0xc0ee1
    push si                                   ; 56                          ; 0xc0ee3
    push di                                   ; 57                          ; 0xc0ee4
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0ee5
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0ee8
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc0eeb
    mov si, cx                                ; 89 ce                       ; 0xc0eee
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0ef0 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ef3
    mov es, ax                                ; 8e c0                       ; 0xc0ef6
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ef8
    xor ah, ah                                ; 30 e4                       ; 0xc0efb vgabios.c:615
    call 033c0h                               ; e8 c0 24                    ; 0xc0efd
    mov ah, al                                ; 88 c4                       ; 0xc0f00
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f02 vgabios.c:616
    je near 0102fh                            ; 0f 84 27 01                 ; 0xc0f04
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0f08 vgabios.c:618
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0f0b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f0e
    je near 0102fh                            ; 0f 84 18 01                 ; 0xc0f13
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f17 vgabios.c:622
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0f1b
    jc short 00f31h                           ; 72 11                       ; 0xc0f1e
    jbe short 00f39h                          ; 76 17                       ; 0xc0f20
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f22
    je near 01008h                            ; 0f 84 df 00                 ; 0xc0f25
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f29
    je short 00f39h                           ; 74 0b                       ; 0xc0f2c
    jmp near 01028h                           ; e9 f7 00                    ; 0xc0f2e
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f31
    je short 00fa4h                           ; 74 6e                       ; 0xc0f34
    jmp near 01028h                           ; e9 ef 00                    ; 0xc0f36
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0f39 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f3c
    mov es, ax                                ; 8e c0                       ; 0xc0f3f
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0f41
    imul ax, word [bp-00ch]                   ; 0f af 46 f4                 ; 0xc0f44 vgabios.c:48
    mov bx, dx                                ; 89 d3                       ; 0xc0f48
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0f4a
    add bx, ax                                ; 01 c3                       ; 0xc0f4d
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0f4f vgabios.c:47
    mov cx, word [es:di]                      ; 26 8b 0d                    ; 0xc0f52
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc0f55 vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc0f59
    add bx, ax                                ; 01 c3                       ; 0xc0f5c
    mov cl, dl                                ; 88 d1                       ; 0xc0f5e vgabios.c:627
    and cl, 007h                              ; 80 e1 07                    ; 0xc0f60
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0f63
    sar ax, CL                                ; d3 f8                       ; 0xc0f66
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0f68
    xor ch, ch                                ; 30 ed                       ; 0xc0f6b vgabios.c:628
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0f6d vgabios.c:629
    jmp short 00f7ah                          ; eb 08                       ; 0xc0f70
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0f72
    jnc near 0102ah                           ; 0f 83 b0 00                 ; 0xc0f76
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc0f7a vgabios.c:630
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0f7e
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0f81
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0f83
    out DX, ax                                ; ef                          ; 0xc0f86
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0f87 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0f8a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f8c
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0f8f vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0f92 vgabios.c:632
    jbe short 00f9fh                          ; 76 09                       ; 0xc0f94
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0f96 vgabios.c:633
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0f99
    sal al, CL                                ; d2 e0                       ; 0xc0f9b
    or ch, al                                 ; 08 c5                       ; 0xc0f9d
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0f9f vgabios.c:634
    jmp short 00f72h                          ; eb ce                       ; 0xc0fa2
    movzx cx, byte [bx+047b1h]                ; 0f b6 8f b1 47              ; 0xc0fa4 vgabios.c:637
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc0fa9
    sub bx, cx                                ; 29 cb                       ; 0xc0fac
    mov cx, bx                                ; 89 d9                       ; 0xc0fae
    mov bx, dx                                ; 89 d3                       ; 0xc0fb0
    shr bx, CL                                ; d3 eb                       ; 0xc0fb2
    mov cx, bx                                ; 89 d9                       ; 0xc0fb4
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc0fb6
    shr bx, 1                                 ; d1 eb                       ; 0xc0fb9
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc0fbb
    add bx, cx                                ; 01 cb                       ; 0xc0fbe
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc0fc0 vgabios.c:638
    je short 00fc9h                           ; 74 03                       ; 0xc0fc4
    add bh, 020h                              ; 80 c7 20                    ; 0xc0fc6 vgabios.c:639
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc0fc9 vgabios.c:37
    mov es, cx                                ; 8e c1                       ; 0xc0fcc
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fce
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc0fd1 vgabios.c:641
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0fd4
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc0fd7
    jne short 00ff3h                          ; 75 15                       ; 0xc0fdc
    and dx, strict byte 00003h                ; 83 e2 03                    ; 0xc0fde vgabios.c:642
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc0fe1
    sub cx, dx                                ; 29 d1                       ; 0xc0fe4
    add cx, cx                                ; 01 c9                       ; 0xc0fe6
    xor ah, ah                                ; 30 e4                       ; 0xc0fe8
    sar ax, CL                                ; d3 f8                       ; 0xc0fea
    mov ch, al                                ; 88 c5                       ; 0xc0fec
    and ch, 003h                              ; 80 e5 03                    ; 0xc0fee
    jmp short 0102ah                          ; eb 37                       ; 0xc0ff1 vgabios.c:643
    xor dh, dh                                ; 30 f6                       ; 0xc0ff3 vgabios.c:644
    and dl, 007h                              ; 80 e2 07                    ; 0xc0ff5
    mov cx, strict word 00007h                ; b9 07 00                    ; 0xc0ff8
    sub cx, dx                                ; 29 d1                       ; 0xc0ffb
    xor ah, ah                                ; 30 e4                       ; 0xc0ffd
    sar ax, CL                                ; d3 f8                       ; 0xc0fff
    mov ch, al                                ; 88 c5                       ; 0xc1001
    and ch, 001h                              ; 80 e5 01                    ; 0xc1003
    jmp short 0102ah                          ; eb 22                       ; 0xc1006 vgabios.c:645
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1008 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc100b
    mov es, ax                                ; 8e c0                       ; 0xc100e
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1010
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1013 vgabios.c:48
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1016
    imul bx, ax                               ; 0f af d8                    ; 0xc1019
    add bx, dx                                ; 01 d3                       ; 0xc101c
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc101e vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc1021
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc1023
    jmp short 0102ah                          ; eb 02                       ; 0xc1026 vgabios.c:649
    xor ch, ch                                ; 30 ed                       ; 0xc1028 vgabios.c:654
    push SS                                   ; 16                          ; 0xc102a vgabios.c:656
    pop ES                                    ; 07                          ; 0xc102b
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc102c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc102f vgabios.c:657
    pop di                                    ; 5f                          ; 0xc1032
    pop si                                    ; 5e                          ; 0xc1033
    pop bp                                    ; 5d                          ; 0xc1034
    retn                                      ; c3                          ; 0xc1035
  ; disGetNextSymbol 0xc1036 LB 0x2fef -> off=0x0 cb=000000000000008c uValue=00000000000c1036 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc1036 LB 0x8c
    push bp                                   ; 55                          ; 0xc1036 vgabios.c:662
    mov bp, sp                                ; 89 e5                       ; 0xc1037
    push bx                                   ; 53                          ; 0xc1039
    push cx                                   ; 51                          ; 0xc103a
    push si                                   ; 56                          ; 0xc103b
    push di                                   ; 57                          ; 0xc103c
    push ax                                   ; 50                          ; 0xc103d
    push ax                                   ; 50                          ; 0xc103e
    mov bx, ax                                ; 89 c3                       ; 0xc103f
    mov di, dx                                ; 89 d7                       ; 0xc1041
    mov dx, 003dah                            ; ba da 03                    ; 0xc1043 vgabios.c:667
    in AL, DX                                 ; ec                          ; 0xc1046
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1047
    xor al, al                                ; 30 c0                       ; 0xc1049 vgabios.c:668
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc104b
    out DX, AL                                ; ee                          ; 0xc104e
    xor si, si                                ; 31 f6                       ; 0xc104f vgabios.c:670
    cmp si, di                                ; 39 fe                       ; 0xc1051
    jnc short 010a7h                          ; 73 52                       ; 0xc1053
    mov al, bl                                ; 88 d8                       ; 0xc1055 vgabios.c:673
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc1057
    out DX, AL                                ; ee                          ; 0xc105a
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc105b vgabios.c:675
    in AL, DX                                 ; ec                          ; 0xc105e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc105f
    mov cx, ax                                ; 89 c1                       ; 0xc1061
    in AL, DX                                 ; ec                          ; 0xc1063 vgabios.c:676
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1064
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1066
    in AL, DX                                 ; ec                          ; 0xc1069 vgabios.c:677
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc106a
    xor ch, ch                                ; 30 ed                       ; 0xc106c vgabios.c:680
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc106e
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc1071
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4                 ; 0xc1074
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc1078
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc107c
    xor ah, ah                                ; 30 e4                       ; 0xc107f
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc1081
    add cx, ax                                ; 01 c1                       ; 0xc1084
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc1086
    sar cx, 008h                              ; c1 f9 08                    ; 0xc108a
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc108d vgabios.c:682
    jbe short 01095h                          ; 76 03                       ; 0xc1090
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc1092
    mov al, bl                                ; 88 d8                       ; 0xc1095 vgabios.c:685
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1097
    out DX, AL                                ; ee                          ; 0xc109a
    mov al, cl                                ; 88 c8                       ; 0xc109b vgabios.c:687
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc109d
    out DX, AL                                ; ee                          ; 0xc10a0
    out DX, AL                                ; ee                          ; 0xc10a1 vgabios.c:688
    out DX, AL                                ; ee                          ; 0xc10a2 vgabios.c:689
    inc bx                                    ; 43                          ; 0xc10a3 vgabios.c:690
    inc si                                    ; 46                          ; 0xc10a4 vgabios.c:691
    jmp short 01051h                          ; eb aa                       ; 0xc10a5
    mov dx, 003dah                            ; ba da 03                    ; 0xc10a7 vgabios.c:692
    in AL, DX                                 ; ec                          ; 0xc10aa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10ab
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc10ad vgabios.c:693
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10af
    out DX, AL                                ; ee                          ; 0xc10b2
    mov dx, 003dah                            ; ba da 03                    ; 0xc10b3 vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc10b6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10b7
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc10b9 vgabios.c:697
    pop di                                    ; 5f                          ; 0xc10bc
    pop si                                    ; 5e                          ; 0xc10bd
    pop cx                                    ; 59                          ; 0xc10be
    pop bx                                    ; 5b                          ; 0xc10bf
    pop bp                                    ; 5d                          ; 0xc10c0
    retn                                      ; c3                          ; 0xc10c1
  ; disGetNextSymbol 0xc10c2 LB 0x2f63 -> off=0x0 cb=00000000000000f6 uValue=00000000000c10c2 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc10c2 LB 0xf6
    push bp                                   ; 55                          ; 0xc10c2 vgabios.c:700
    mov bp, sp                                ; 89 e5                       ; 0xc10c3
    push bx                                   ; 53                          ; 0xc10c5
    push cx                                   ; 51                          ; 0xc10c6
    push si                                   ; 56                          ; 0xc10c7
    push di                                   ; 57                          ; 0xc10c8
    push ax                                   ; 50                          ; 0xc10c9
    mov bl, al                                ; 88 c3                       ; 0xc10ca
    mov ah, dl                                ; 88 d4                       ; 0xc10cc
    movzx cx, al                              ; 0f b6 c8                    ; 0xc10ce vgabios.c:706
    sal cx, 008h                              ; c1 e1 08                    ; 0xc10d1
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc10d4
    add dx, cx                                ; 01 ca                       ; 0xc10d7
    mov si, strict word 00060h                ; be 60 00                    ; 0xc10d9 vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc10dc
    mov es, cx                                ; 8e c1                       ; 0xc10df
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc10e1
    mov si, 00087h                            ; be 87 00                    ; 0xc10e4 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc10e7
    test dl, 008h                             ; f6 c2 08                    ; 0xc10ea vgabios.c:38
    jne near 0118dh                           ; 0f 85 9c 00                 ; 0xc10ed
    mov dl, al                                ; 88 c2                       ; 0xc10f1 vgabios.c:712
    and dl, 060h                              ; 80 e2 60                    ; 0xc10f3
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc10f6
    jne short 01102h                          ; 75 07                       ; 0xc10f9
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc10fb vgabios.c:714
    xor ah, ah                                ; 30 e4                       ; 0xc10fd vgabios.c:715
    jmp near 0118dh                           ; e9 8b 00                    ; 0xc10ff vgabios.c:716
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1102 vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc1105 vgabios.c:38
    jne near 0118dh                           ; 0f 85 81 00                 ; 0xc1108
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc110c
    jnc near 0118dh                           ; 0f 83 7a 00                 ; 0xc110f
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc1113
    jnc near 0118dh                           ; 0f 83 73 00                 ; 0xc1116
    mov si, 00085h                            ; be 85 00                    ; 0xc111a vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc111d
    mov es, dx                                ; 8e c2                       ; 0xc1120
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1122
    mov dx, cx                                ; 89 ca                       ; 0xc1125 vgabios.c:48
    cmp ah, bl                                ; 38 dc                       ; 0xc1127 vgabios.c:727
    jnc short 01137h                          ; 73 0c                       ; 0xc1129
    test ah, ah                               ; 84 e4                       ; 0xc112b vgabios.c:729
    je short 0118dh                           ; 74 5e                       ; 0xc112d
    xor bl, bl                                ; 30 db                       ; 0xc112f vgabios.c:730
    mov ah, cl                                ; 88 cc                       ; 0xc1131 vgabios.c:731
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1133
    jmp short 0118dh                          ; eb 56                       ; 0xc1135 vgabios.c:733
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1137 vgabios.c:734
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc113a
    movzx si, bl                              ; 0f b6 f3                    ; 0xc113d
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc1140
    cmp si, cx                                ; 39 ce                       ; 0xc1143
    jnc short 0115ah                          ; 73 13                       ; 0xc1145
    movzx di, ah                              ; 0f b6 fc                    ; 0xc1147
    mov si, cx                                ; 89 ce                       ; 0xc114a
    dec si                                    ; 4e                          ; 0xc114c
    cmp di, si                                ; 39 f7                       ; 0xc114d
    je short 0118dh                           ; 74 3c                       ; 0xc114f
    movzx si, bl                              ; 0f b6 f3                    ; 0xc1151
    dec cx                                    ; 49                          ; 0xc1154
    dec cx                                    ; 49                          ; 0xc1155
    cmp si, cx                                ; 39 ce                       ; 0xc1156
    je short 0118dh                           ; 74 33                       ; 0xc1158
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc115a vgabios.c:736
    jbe short 0118dh                          ; 76 2e                       ; 0xc115d
    movzx si, bl                              ; 0f b6 f3                    ; 0xc115f vgabios.c:737
    movzx di, ah                              ; 0f b6 fc                    ; 0xc1162
    inc si                                    ; 46                          ; 0xc1165
    inc si                                    ; 46                          ; 0xc1166
    mov cl, dl                                ; 88 d1                       ; 0xc1167
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc1169
    cmp di, si                                ; 39 f7                       ; 0xc116b
    jnle short 01182h                         ; 7f 13                       ; 0xc116d
    sub bl, ah                                ; 28 e3                       ; 0xc116f vgabios.c:739
    add bl, dl                                ; 00 d3                       ; 0xc1171
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1173
    mov ah, cl                                ; 88 cc                       ; 0xc1175 vgabios.c:740
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc1177 vgabios.c:741
    jc short 0118dh                           ; 72 11                       ; 0xc117a
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc117c vgabios.c:743
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc117e vgabios.c:744
    jmp short 0118dh                          ; eb 0b                       ; 0xc1180 vgabios.c:746
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1182
    jbe short 0118bh                          ; 76 04                       ; 0xc1185
    shr dx, 1                                 ; d1 ea                       ; 0xc1187 vgabios.c:748
    mov bl, dl                                ; 88 d3                       ; 0xc1189
    mov ah, cl                                ; 88 cc                       ; 0xc118b vgabios.c:752
    mov si, strict word 00063h                ; be 63 00                    ; 0xc118d vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1190
    mov es, dx                                ; 8e c2                       ; 0xc1193
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1195
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc1198 vgabios.c:763
    mov dx, cx                                ; 89 ca                       ; 0xc119a
    out DX, AL                                ; ee                          ; 0xc119c
    mov si, cx                                ; 89 ce                       ; 0xc119d vgabios.c:764
    inc si                                    ; 46                          ; 0xc119f
    mov al, bl                                ; 88 d8                       ; 0xc11a0
    mov dx, si                                ; 89 f2                       ; 0xc11a2
    out DX, AL                                ; ee                          ; 0xc11a4
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc11a5 vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc11a7
    out DX, AL                                ; ee                          ; 0xc11a9
    mov al, ah                                ; 88 e0                       ; 0xc11aa vgabios.c:766
    mov dx, si                                ; 89 f2                       ; 0xc11ac
    out DX, AL                                ; ee                          ; 0xc11ae
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc11af vgabios.c:767
    pop di                                    ; 5f                          ; 0xc11b2
    pop si                                    ; 5e                          ; 0xc11b3
    pop cx                                    ; 59                          ; 0xc11b4
    pop bx                                    ; 5b                          ; 0xc11b5
    pop bp                                    ; 5d                          ; 0xc11b6
    retn                                      ; c3                          ; 0xc11b7
  ; disGetNextSymbol 0xc11b8 LB 0x2e6d -> off=0x0 cb=0000000000000089 uValue=00000000000c11b8 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc11b8 LB 0x89
    push bp                                   ; 55                          ; 0xc11b8 vgabios.c:770
    mov bp, sp                                ; 89 e5                       ; 0xc11b9
    push bx                                   ; 53                          ; 0xc11bb
    push cx                                   ; 51                          ; 0xc11bc
    push si                                   ; 56                          ; 0xc11bd
    push ax                                   ; 50                          ; 0xc11be
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc11bf vgabios.c:776
    jnbe short 01239h                         ; 77 76                       ; 0xc11c1
    movzx bx, al                              ; 0f b6 d8                    ; 0xc11c3 vgabios.c:779
    add bx, bx                                ; 01 db                       ; 0xc11c6
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc11c8
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc11cb vgabios.c:52
    mov es, cx                                ; 8e c1                       ; 0xc11ce
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc11d0
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc11d3 vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc11d6
    cmp al, ah                                ; 38 e0                       ; 0xc11d9 vgabios.c:783
    jne short 01239h                          ; 75 5c                       ; 0xc11db
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc11dd vgabios.c:47
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc11e0
    mov bx, 00084h                            ; bb 84 00                    ; 0xc11e3 vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc11e6
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc11e9 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc11ec
    mov si, dx                                ; 89 d6                       ; 0xc11ed vgabios.c:789
    and si, 0ff00h                            ; 81 e6 00 ff                 ; 0xc11ef
    shr si, 008h                              ; c1 ee 08                    ; 0xc11f3
    mov word [bp-008h], si                    ; 89 76 f8                    ; 0xc11f6
    imul bx, cx                               ; 0f af d9                    ; 0xc11f9 vgabios.c:792
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc11fc
    xor ah, ah                                ; 30 e4                       ; 0xc11ff
    inc bx                                    ; 43                          ; 0xc1201
    imul ax, bx                               ; 0f af c3                    ; 0xc1202
    movzx si, dl                              ; 0f b6 f2                    ; 0xc1205
    add si, ax                                ; 01 c6                       ; 0xc1208
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc120a
    imul ax, cx                               ; 0f af c1                    ; 0xc120e
    add si, ax                                ; 01 c6                       ; 0xc1211
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1213 vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1216
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc1219 vgabios.c:796
    mov dx, bx                                ; 89 da                       ; 0xc121b
    out DX, AL                                ; ee                          ; 0xc121d
    mov ax, si                                ; 89 f0                       ; 0xc121e vgabios.c:797
    xor al, al                                ; 30 c0                       ; 0xc1220
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1222
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc1225
    mov dx, cx                                ; 89 ca                       ; 0xc1228
    out DX, AL                                ; ee                          ; 0xc122a
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc122b vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc122d
    out DX, AL                                ; ee                          ; 0xc122f
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc1230 vgabios.c:799
    mov ax, si                                ; 89 f0                       ; 0xc1234
    mov dx, cx                                ; 89 ca                       ; 0xc1236
    out DX, AL                                ; ee                          ; 0xc1238
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc1239 vgabios.c:801
    pop si                                    ; 5e                          ; 0xc123c
    pop cx                                    ; 59                          ; 0xc123d
    pop bx                                    ; 5b                          ; 0xc123e
    pop bp                                    ; 5d                          ; 0xc123f
    retn                                      ; c3                          ; 0xc1240
  ; disGetNextSymbol 0xc1241 LB 0x2de4 -> off=0x0 cb=00000000000000cd uValue=00000000000c1241 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc1241 LB 0xcd
    push bp                                   ; 55                          ; 0xc1241 vgabios.c:804
    mov bp, sp                                ; 89 e5                       ; 0xc1242
    push bx                                   ; 53                          ; 0xc1244
    push cx                                   ; 51                          ; 0xc1245
    push dx                                   ; 52                          ; 0xc1246
    push si                                   ; 56                          ; 0xc1247
    push di                                   ; 57                          ; 0xc1248
    push ax                                   ; 50                          ; 0xc1249
    push ax                                   ; 50                          ; 0xc124a
    mov cl, al                                ; 88 c1                       ; 0xc124b
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc124d vgabios.c:810
    jnbe near 01304h                          ; 0f 87 b1 00                 ; 0xc124f
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1253 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1256
    mov es, ax                                ; 8e c0                       ; 0xc1259
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc125b
    xor ah, ah                                ; 30 e4                       ; 0xc125e vgabios.c:814
    call 033c0h                               ; e8 5d 21                    ; 0xc1260
    mov ch, al                                ; 88 c5                       ; 0xc1263
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1265 vgabios.c:815
    je near 01304h                            ; 0f 84 99 00                 ; 0xc1267
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc126b vgabios.c:818
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc126e
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc1271
    call 00a08h                               ; e8 91 f7                    ; 0xc1274
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1277 vgabios.c:820
    mov si, bx                                ; 89 de                       ; 0xc127a
    sal si, 003h                              ; c1 e6 03                    ; 0xc127c
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc127f
    jne short 012bah                          ; 75 34                       ; 0xc1284
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1286 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1289
    mov es, ax                                ; 8e c0                       ; 0xc128c
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc128e
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1291 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1294
    xor ah, ah                                ; 30 e4                       ; 0xc1297 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc1299
    imul dx, ax                               ; 0f af d0                    ; 0xc129a vgabios.c:827
    mov ax, dx                                ; 89 d0                       ; 0xc129d
    add ax, dx                                ; 01 d0                       ; 0xc129f
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc12a1
    mov bx, ax                                ; 89 c3                       ; 0xc12a3
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12a5
    inc bx                                    ; 43                          ; 0xc12a8
    imul bx, ax                               ; 0f af d8                    ; 0xc12a9
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc12ac vgabios.c:52
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc12af
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc12b2 vgabios.c:831
    mov bx, dx                                ; 89 d3                       ; 0xc12b5
    inc bx                                    ; 43                          ; 0xc12b7
    jmp short 012c9h                          ; eb 0f                       ; 0xc12b8 vgabios.c:833
    movzx bx, byte [bx+0482eh]                ; 0f b6 9f 2e 48              ; 0xc12ba vgabios.c:835
    sal bx, 006h                              ; c1 e3 06                    ; 0xc12bf
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12c2
    mov bx, word [bx+04845h]                  ; 8b 9f 45 48                 ; 0xc12c5
    imul bx, ax                               ; 0f af d8                    ; 0xc12c9
    mov si, strict word 00063h                ; be 63 00                    ; 0xc12cc vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12cf
    mov es, ax                                ; 8e c0                       ; 0xc12d2
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc12d4
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc12d7 vgabios.c:840
    mov dx, si                                ; 89 f2                       ; 0xc12d9
    out DX, AL                                ; ee                          ; 0xc12db
    mov ax, bx                                ; 89 d8                       ; 0xc12dc vgabios.c:841
    xor al, bl                                ; 30 d8                       ; 0xc12de
    shr ax, 008h                              ; c1 e8 08                    ; 0xc12e0
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc12e3
    mov dx, di                                ; 89 fa                       ; 0xc12e6
    out DX, AL                                ; ee                          ; 0xc12e8
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc12e9 vgabios.c:842
    mov dx, si                                ; 89 f2                       ; 0xc12eb
    out DX, AL                                ; ee                          ; 0xc12ed
    xor bh, bh                                ; 30 ff                       ; 0xc12ee vgabios.c:843
    mov ax, bx                                ; 89 d8                       ; 0xc12f0
    mov dx, di                                ; 89 fa                       ; 0xc12f2
    out DX, AL                                ; ee                          ; 0xc12f4
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc12f5 vgabios.c:42
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc12f8
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc12fb vgabios.c:853
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12fe
    call 011b8h                               ; e8 b4 fe                    ; 0xc1301
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1304 vgabios.c:854
    pop di                                    ; 5f                          ; 0xc1307
    pop si                                    ; 5e                          ; 0xc1308
    pop dx                                    ; 5a                          ; 0xc1309
    pop cx                                    ; 59                          ; 0xc130a
    pop bx                                    ; 5b                          ; 0xc130b
    pop bp                                    ; 5d                          ; 0xc130c
    retn                                      ; c3                          ; 0xc130d
  ; disGetNextSymbol 0xc130e LB 0x2d17 -> off=0x0 cb=0000000000000369 uValue=00000000000c130e 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc130e LB 0x369
    push bp                                   ; 55                          ; 0xc130e vgabios.c:874
    mov bp, sp                                ; 89 e5                       ; 0xc130f
    push bx                                   ; 53                          ; 0xc1311
    push cx                                   ; 51                          ; 0xc1312
    push dx                                   ; 52                          ; 0xc1313
    push si                                   ; 56                          ; 0xc1314
    push di                                   ; 57                          ; 0xc1315
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc1316
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1319
    and AL, strict byte 080h                  ; 24 80                       ; 0xc131c vgabios.c:878
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc131e
    call 007bfh                               ; e8 9b f4                    ; 0xc1321 vgabios.c:885
    test ax, ax                               ; 85 c0                       ; 0xc1324
    je short 01334h                           ; 74 0c                       ; 0xc1326
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1328 vgabios.c:887
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc132a
    out DX, AL                                ; ee                          ; 0xc132d
    xor al, al                                ; 30 c0                       ; 0xc132e vgabios.c:888
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1330
    out DX, AL                                ; ee                          ; 0xc1333
    and byte [bp-00eh], 07fh                  ; 80 66 f2 7f                 ; 0xc1334 vgabios.c:893
    cmp byte [bp-00eh], 007h                  ; 80 7e f2 07                 ; 0xc1338 vgabios.c:897
    jne short 01342h                          ; 75 04                       ; 0xc133c
    mov byte [bp-00eh], 000h                  ; c6 46 f2 00                 ; 0xc133e vgabios.c:898
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1342 vgabios.c:901
    call 033c0h                               ; e8 77 20                    ; 0xc1346
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1349
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc134c vgabios.c:907
    je near 0166dh                            ; 0f 84 1b 03                 ; 0xc134e
    movzx di, al                              ; 0f b6 f8                    ; 0xc1352 vgabios.c:910
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc1355
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1359
    movzx bx, al                              ; 0f b6 d8                    ; 0xc135c vgabios.c:911
    sal bx, 006h                              ; c1 e3 06                    ; 0xc135f
    movzx ax, byte [bx+04842h]                ; 0f b6 87 42 48              ; 0xc1362
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1367
    movzx ax, byte [bx+04843h]                ; 0f b6 87 43 48              ; 0xc136a vgabios.c:912
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc136f
    movzx ax, byte [bx+04844h]                ; 0f b6 87 44 48              ; 0xc1372 vgabios.c:913
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1377
    mov bx, 00089h                            ; bb 89 00                    ; 0xc137a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc137d
    mov es, ax                                ; 8e c0                       ; 0xc1380
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1382
    mov ah, al                                ; 88 c4                       ; 0xc1385 vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1387 vgabios.c:928
    jne near 01417h                           ; 0f 85 8a 00                 ; 0xc1389
    mov bx, di                                ; 89 fb                       ; 0xc138d vgabios.c:930
    sal bx, 003h                              ; c1 e3 03                    ; 0xc138f
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc1392
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1396
    out DX, AL                                ; ee                          ; 0xc1399
    xor al, al                                ; 30 c0                       ; 0xc139a vgabios.c:933
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc139c
    out DX, AL                                ; ee                          ; 0xc139f
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc13a0 vgabios.c:936
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc13a4
    jc short 013b7h                           ; 72 0e                       ; 0xc13a7
    jbe short 013c0h                          ; 76 15                       ; 0xc13a9
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc13ab
    je short 013cah                           ; 74 1a                       ; 0xc13ae
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc13b0
    je short 013c5h                           ; 74 10                       ; 0xc13b3
    jmp short 013cdh                          ; eb 16                       ; 0xc13b5
    test bl, bl                               ; 84 db                       ; 0xc13b7
    jne short 013cdh                          ; 75 12                       ; 0xc13b9
    mov si, 04fc2h                            ; be c2 4f                    ; 0xc13bb vgabios.c:938
    jmp short 013cdh                          ; eb 0d                       ; 0xc13be vgabios.c:939
    mov si, 05082h                            ; be 82 50                    ; 0xc13c0 vgabios.c:941
    jmp short 013cdh                          ; eb 08                       ; 0xc13c3 vgabios.c:942
    mov si, 05142h                            ; be 42 51                    ; 0xc13c5 vgabios.c:944
    jmp short 013cdh                          ; eb 03                       ; 0xc13c8 vgabios.c:945
    mov si, 05202h                            ; be 02 52                    ; 0xc13ca vgabios.c:947
    xor cx, cx                                ; 31 c9                       ; 0xc13cd vgabios.c:951
    jmp short 013e0h                          ; eb 0f                       ; 0xc13cf
    xor al, al                                ; 30 c0                       ; 0xc13d1 vgabios.c:958
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc13d3
    out DX, AL                                ; ee                          ; 0xc13d6
    out DX, AL                                ; ee                          ; 0xc13d7 vgabios.c:959
    out DX, AL                                ; ee                          ; 0xc13d8 vgabios.c:960
    inc cx                                    ; 41                          ; 0xc13d9 vgabios.c:962
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc13da
    jnc short 0140ah                          ; 73 2a                       ; 0xc13de
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc13e0
    sal bx, 003h                              ; c1 e3 03                    ; 0xc13e4
    movzx bx, byte [bx+047b5h]                ; 0f b6 9f b5 47              ; 0xc13e7
    movzx dx, byte [bx+0483eh]                ; 0f b6 97 3e 48              ; 0xc13ec
    cmp cx, dx                                ; 39 d1                       ; 0xc13f1
    jnbe short 013d1h                         ; 77 dc                       ; 0xc13f3
    imul bx, cx, strict byte 00003h           ; 6b d9 03                    ; 0xc13f5
    add bx, si                                ; 01 f3                       ; 0xc13f8
    mov al, byte [bx]                         ; 8a 07                       ; 0xc13fa
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc13fc
    out DX, AL                                ; ee                          ; 0xc13ff
    mov al, byte [bx+001h]                    ; 8a 47 01                    ; 0xc1400
    out DX, AL                                ; ee                          ; 0xc1403
    mov al, byte [bx+002h]                    ; 8a 47 02                    ; 0xc1404
    out DX, AL                                ; ee                          ; 0xc1407
    jmp short 013d9h                          ; eb cf                       ; 0xc1408
    test ah, 002h                             ; f6 c4 02                    ; 0xc140a vgabios.c:963
    je short 01417h                           ; 74 08                       ; 0xc140d
    mov dx, 00100h                            ; ba 00 01                    ; 0xc140f vgabios.c:965
    xor ax, ax                                ; 31 c0                       ; 0xc1412
    call 01036h                               ; e8 1f fc                    ; 0xc1414
    mov dx, 003dah                            ; ba da 03                    ; 0xc1417 vgabios.c:970
    in AL, DX                                 ; ec                          ; 0xc141a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc141b
    xor cx, cx                                ; 31 c9                       ; 0xc141d vgabios.c:973
    jmp short 01426h                          ; eb 05                       ; 0xc141f
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc1421
    jnbe short 0143dh                         ; 77 17                       ; 0xc1424
    mov al, cl                                ; 88 c8                       ; 0xc1426 vgabios.c:974
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1428
    out DX, AL                                ; ee                          ; 0xc142b
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc142c vgabios.c:975
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1430
    add bx, cx                                ; 01 cb                       ; 0xc1433
    mov al, byte [bx+04865h]                  ; 8a 87 65 48                 ; 0xc1435
    out DX, AL                                ; ee                          ; 0xc1439
    inc cx                                    ; 41                          ; 0xc143a vgabios.c:976
    jmp short 01421h                          ; eb e4                       ; 0xc143b
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc143d vgabios.c:977
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc143f
    out DX, AL                                ; ee                          ; 0xc1442
    xor al, al                                ; 30 c0                       ; 0xc1443 vgabios.c:978
    out DX, AL                                ; ee                          ; 0xc1445
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1446 vgabios.c:981
    out DX, AL                                ; ee                          ; 0xc1449
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc144a vgabios.c:982
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc144c
    out DX, AL                                ; ee                          ; 0xc144f
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc1450 vgabios.c:983
    jmp short 0145ah                          ; eb 05                       ; 0xc1453
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc1455
    jnbe short 01474h                         ; 77 1a                       ; 0xc1458
    mov al, cl                                ; 88 c8                       ; 0xc145a vgabios.c:984
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc145c
    out DX, AL                                ; ee                          ; 0xc145f
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1460 vgabios.c:985
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1464
    add bx, cx                                ; 01 cb                       ; 0xc1467
    mov al, byte [bx+04846h]                  ; 8a 87 46 48                 ; 0xc1469
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc146d
    out DX, AL                                ; ee                          ; 0xc1470
    inc cx                                    ; 41                          ; 0xc1471 vgabios.c:986
    jmp short 01455h                          ; eb e1                       ; 0xc1472
    xor cx, cx                                ; 31 c9                       ; 0xc1474 vgabios.c:989
    jmp short 0147dh                          ; eb 05                       ; 0xc1476
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc1478
    jnbe short 01497h                         ; 77 1a                       ; 0xc147b
    mov al, cl                                ; 88 c8                       ; 0xc147d vgabios.c:990
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc147f
    out DX, AL                                ; ee                          ; 0xc1482
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1483 vgabios.c:991
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1487
    add bx, cx                                ; 01 cb                       ; 0xc148a
    mov al, byte [bx+04879h]                  ; 8a 87 79 48                 ; 0xc148c
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc1490
    out DX, AL                                ; ee                          ; 0xc1493
    inc cx                                    ; 41                          ; 0xc1494 vgabios.c:992
    jmp short 01478h                          ; eb e1                       ; 0xc1495
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1497 vgabios.c:995
    sal bx, 003h                              ; c1 e3 03                    ; 0xc149b
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc149e
    jne short 014aah                          ; 75 05                       ; 0xc14a3
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc14a5
    jmp short 014adh                          ; eb 03                       ; 0xc14a8
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc14aa
    mov si, dx                                ; 89 d6                       ; 0xc14ad
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc14af vgabios.c:998
    out DX, ax                                ; ef                          ; 0xc14b2
    xor cx, cx                                ; 31 c9                       ; 0xc14b3 vgabios.c:1000
    jmp short 014bch                          ; eb 05                       ; 0xc14b5
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc14b7
    jnbe short 014d7h                         ; 77 1b                       ; 0xc14ba
    mov al, cl                                ; 88 c8                       ; 0xc14bc vgabios.c:1001
    mov dx, si                                ; 89 f2                       ; 0xc14be
    out DX, AL                                ; ee                          ; 0xc14c0
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc14c1 vgabios.c:1002
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14c5
    mov di, bx                                ; 89 df                       ; 0xc14c8
    add di, cx                                ; 01 cf                       ; 0xc14ca
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc14cc
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc14cf
    out DX, AL                                ; ee                          ; 0xc14d3
    inc cx                                    ; 41                          ; 0xc14d4 vgabios.c:1003
    jmp short 014b7h                          ; eb e0                       ; 0xc14d5
    mov al, byte [bx+0484bh]                  ; 8a 87 4b 48                 ; 0xc14d7 vgabios.c:1006
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc14db
    out DX, AL                                ; ee                          ; 0xc14de
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc14df vgabios.c:1009
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14e1
    out DX, AL                                ; ee                          ; 0xc14e4
    mov dx, 003dah                            ; ba da 03                    ; 0xc14e5 vgabios.c:1010
    in AL, DX                                 ; ec                          ; 0xc14e8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc14e9
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc14eb vgabios.c:1012
    jne short 01550h                          ; 75 5f                       ; 0xc14ef
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc14f1 vgabios.c:1014
    sal bx, 003h                              ; c1 e3 03                    ; 0xc14f5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc14f8
    jne short 01512h                          ; 75 13                       ; 0xc14fd
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc14ff vgabios.c:1016
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1503
    mov ax, 00720h                            ; b8 20 07                    ; 0xc1506
    xor di, di                                ; 31 ff                       ; 0xc1509
    cld                                       ; fc                          ; 0xc150b
    jcxz 01510h                               ; e3 02                       ; 0xc150c
    rep stosw                                 ; f3 ab                       ; 0xc150e
    jmp short 01550h                          ; eb 3e                       ; 0xc1510 vgabios.c:1018
    cmp byte [bp-00eh], 00dh                  ; 80 7e f2 0d                 ; 0xc1512 vgabios.c:1020
    jnc short 0152ah                          ; 73 12                       ; 0xc1516
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1518 vgabios.c:1022
    mov cx, 04000h                            ; b9 00 40                    ; 0xc151c
    xor ax, ax                                ; 31 c0                       ; 0xc151f
    xor di, di                                ; 31 ff                       ; 0xc1521
    cld                                       ; fc                          ; 0xc1523
    jcxz 01528h                               ; e3 02                       ; 0xc1524
    rep stosw                                 ; f3 ab                       ; 0xc1526
    jmp short 01550h                          ; eb 26                       ; 0xc1528 vgabios.c:1024
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc152a vgabios.c:1026
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc152c
    out DX, AL                                ; ee                          ; 0xc152f
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1530 vgabios.c:1027
    in AL, DX                                 ; ec                          ; 0xc1533
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1534
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1536
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1539 vgabios.c:1028
    out DX, AL                                ; ee                          ; 0xc153b
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc153c vgabios.c:1029
    mov cx, 08000h                            ; b9 00 80                    ; 0xc1540
    xor ax, ax                                ; 31 c0                       ; 0xc1543
    xor di, di                                ; 31 ff                       ; 0xc1545
    cld                                       ; fc                          ; 0xc1547
    jcxz 0154ch                               ; e3 02                       ; 0xc1548
    rep stosw                                 ; f3 ab                       ; 0xc154a
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc154c vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc154f
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1550 vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1553
    mov es, ax                                ; 8e c0                       ; 0xc1556
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1558
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc155b
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc155e vgabios.c:52
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1561
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1564
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1567 vgabios.c:1038
    sal bx, 006h                              ; c1 e3 06                    ; 0xc156b
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc156e vgabios.c:50
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc1572 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1575
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1578 vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc157b
    mov bx, 00084h                            ; bb 84 00                    ; 0xc157e vgabios.c:42
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1581
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1584
    mov bx, 00085h                            ; bb 85 00                    ; 0xc1587 vgabios.c:52
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc158a
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc158d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1590 vgabios.c:1042
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1593
    mov bx, 00087h                            ; bb 87 00                    ; 0xc1595 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1598
    mov bx, 00088h                            ; bb 88 00                    ; 0xc159b vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc159e
    mov bx, 00089h                            ; bb 89 00                    ; 0xc15a2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc15a5
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc15a8 vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15aa vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc15ad vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc15b0
    mov dx, ds                                ; 8c da                       ; 0xc15b4 vgabios.c:1048
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc15b6 vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc15b9
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc15be
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc15c2 vgabios.c:1050
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc15c5
    jnbe short 015efh                         ; 77 26                       ; 0xc15c7
    movzx bx, al                              ; 0f b6 d8                    ; 0xc15c9 vgabios.c:1052
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc15cc vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc15d0 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15d3
    cmp byte [bp-00eh], 006h                  ; 80 7e f2 06                 ; 0xc15d6 vgabios.c:1053
    jne short 015e1h                          ; 75 05                       ; 0xc15da
    mov dx, strict word 0003fh                ; ba 3f 00                    ; 0xc15dc
    jmp short 015e4h                          ; eb 03                       ; 0xc15df
    mov dx, strict word 00030h                ; ba 30 00                    ; 0xc15e1
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc15e4 vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15e7
    mov es, ax                                ; 8e c0                       ; 0xc15ea
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc15ec
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc15ef vgabios.c:1057
    sal bx, 003h                              ; c1 e3 03                    ; 0xc15f3
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc15f6
    jne short 01606h                          ; 75 09                       ; 0xc15fb
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc15fd vgabios.c:1059
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc1600
    call 010c2h                               ; e8 bc fa                    ; 0xc1603
    xor cx, cx                                ; 31 c9                       ; 0xc1606 vgabios.c:1063
    jmp short 0160fh                          ; eb 05                       ; 0xc1608
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc160a
    jnc short 0161ah                          ; 73 0b                       ; 0xc160d
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc160f vgabios.c:1064
    xor dx, dx                                ; 31 d2                       ; 0xc1612
    call 011b8h                               ; e8 a1 fb                    ; 0xc1614
    inc cx                                    ; 41                          ; 0xc1617
    jmp short 0160ah                          ; eb f0                       ; 0xc1618
    xor ax, ax                                ; 31 c0                       ; 0xc161a vgabios.c:1067
    call 01241h                               ; e8 22 fc                    ; 0xc161c
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc161f vgabios.c:1070
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1623
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1626
    jne short 0163dh                          ; 75 10                       ; 0xc162b
    xor bl, bl                                ; 30 db                       ; 0xc162d vgabios.c:1072
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc162f
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1631
    int 010h                                  ; cd 10                       ; 0xc1633
    xor bl, bl                                ; 30 db                       ; 0xc1635 vgabios.c:1073
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1637
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1639
    int 010h                                  ; cd 10                       ; 0xc163b
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc163d vgabios.c:1077
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc1640
    call 00980h                               ; e8 3a f3                    ; 0xc1643
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1646 vgabios.c:1079
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc1649
    je short 01668h                           ; 74 1a                       ; 0xc164c
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc164e
    je short 01663h                           ; 74 10                       ; 0xc1651
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc1653
    jne short 0166dh                          ; 75 15                       ; 0xc1656
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc1658 vgabios.c:1081
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc165b
    call 00980h                               ; e8 1f f3                    ; 0xc165e
    jmp short 0166dh                          ; eb 0a                       ; 0xc1661 vgabios.c:1082
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc1663 vgabios.c:1084
    jmp short 0165bh                          ; eb f3                       ; 0xc1666
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc1668 vgabios.c:1087
    jmp short 0165bh                          ; eb ee                       ; 0xc166b
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc166d vgabios.c:1090
    pop di                                    ; 5f                          ; 0xc1670
    pop si                                    ; 5e                          ; 0xc1671
    pop dx                                    ; 5a                          ; 0xc1672
    pop cx                                    ; 59                          ; 0xc1673
    pop bx                                    ; 5b                          ; 0xc1674
    pop bp                                    ; 5d                          ; 0xc1675
    retn                                      ; c3                          ; 0xc1676
  ; disGetNextSymbol 0xc1677 LB 0x29ae -> off=0x0 cb=0000000000000076 uValue=00000000000c1677 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1677 LB 0x76
    push bp                                   ; 55                          ; 0xc1677 vgabios.c:1093
    mov bp, sp                                ; 89 e5                       ; 0xc1678
    push si                                   ; 56                          ; 0xc167a
    push di                                   ; 57                          ; 0xc167b
    push ax                                   ; 50                          ; 0xc167c
    push ax                                   ; 50                          ; 0xc167d
    mov bh, cl                                ; 88 cf                       ; 0xc167e
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1680 vgabios.c:1099
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1683
    imul di, cx                               ; 0f af f9                    ; 0xc1687
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc168a
    imul di, si                               ; 0f af fe                    ; 0xc168e
    xor ah, ah                                ; 30 e4                       ; 0xc1691
    add di, ax                                ; 01 c7                       ; 0xc1693
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc1695
    movzx di, bl                              ; 0f b6 fb                    ; 0xc1698 vgabios.c:1100
    imul cx, di                               ; 0f af cf                    ; 0xc169b
    imul cx, si                               ; 0f af ce                    ; 0xc169e
    add cx, ax                                ; 01 c1                       ; 0xc16a1
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc16a3
    mov ax, 00105h                            ; b8 05 01                    ; 0xc16a6 vgabios.c:1101
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc16a9
    out DX, ax                                ; ef                          ; 0xc16ac
    xor bl, bl                                ; 30 db                       ; 0xc16ad vgabios.c:1102
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc16af
    jnc short 016ddh                          ; 73 29                       ; 0xc16b2
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc16b4 vgabios.c:1104
    movzx si, bl                              ; 0f b6 f3                    ; 0xc16b7
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc16ba
    imul ax, si                               ; 0f af c6                    ; 0xc16be
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc16c1
    add si, ax                                ; 01 c6                       ; 0xc16c4
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc16c6
    add di, ax                                ; 01 c7                       ; 0xc16c9
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc16cb
    mov es, dx                                ; 8e c2                       ; 0xc16ce
    cld                                       ; fc                          ; 0xc16d0
    jcxz 016d9h                               ; e3 06                       ; 0xc16d1
    push DS                                   ; 1e                          ; 0xc16d3
    mov ds, dx                                ; 8e da                       ; 0xc16d4
    rep movsb                                 ; f3 a4                       ; 0xc16d6
    pop DS                                    ; 1f                          ; 0xc16d8
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc16d9 vgabios.c:1105
    jmp short 016afh                          ; eb d2                       ; 0xc16db
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc16dd vgabios.c:1106
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc16e0
    out DX, ax                                ; ef                          ; 0xc16e3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc16e4 vgabios.c:1107
    pop di                                    ; 5f                          ; 0xc16e7
    pop si                                    ; 5e                          ; 0xc16e8
    pop bp                                    ; 5d                          ; 0xc16e9
    retn 00004h                               ; c2 04 00                    ; 0xc16ea
  ; disGetNextSymbol 0xc16ed LB 0x2938 -> off=0x0 cb=0000000000000061 uValue=00000000000c16ed 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc16ed LB 0x61
    push bp                                   ; 55                          ; 0xc16ed vgabios.c:1110
    mov bp, sp                                ; 89 e5                       ; 0xc16ee
    push di                                   ; 57                          ; 0xc16f0
    push ax                                   ; 50                          ; 0xc16f1
    push ax                                   ; 50                          ; 0xc16f2
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc16f3
    mov bh, cl                                ; 88 cf                       ; 0xc16f6
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc16f8 vgabios.c:1116
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc16fb
    imul cx, dx                               ; 0f af ca                    ; 0xc16ff
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc1702
    imul dx, cx                               ; 0f af d1                    ; 0xc1705
    xor ah, ah                                ; 30 e4                       ; 0xc1708
    add dx, ax                                ; 01 c2                       ; 0xc170a
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc170c
    mov ax, 00205h                            ; b8 05 02                    ; 0xc170f vgabios.c:1117
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1712
    out DX, ax                                ; ef                          ; 0xc1715
    xor bl, bl                                ; 30 db                       ; 0xc1716 vgabios.c:1118
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1718
    jnc short 0173fh                          ; 73 22                       ; 0xc171b
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc                 ; 0xc171d vgabios.c:1120
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1721
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc1725
    movzx di, bh                              ; 0f b6 ff                    ; 0xc1728
    imul di, dx                               ; 0f af fa                    ; 0xc172b
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc172e
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1731
    mov es, dx                                ; 8e c2                       ; 0xc1734
    cld                                       ; fc                          ; 0xc1736
    jcxz 0173bh                               ; e3 02                       ; 0xc1737
    rep stosb                                 ; f3 aa                       ; 0xc1739
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc173b vgabios.c:1121
    jmp short 01718h                          ; eb d9                       ; 0xc173d
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc173f vgabios.c:1122
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1742
    out DX, ax                                ; ef                          ; 0xc1745
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc1746 vgabios.c:1123
    pop di                                    ; 5f                          ; 0xc1749
    pop bp                                    ; 5d                          ; 0xc174a
    retn 00004h                               ; c2 04 00                    ; 0xc174b
  ; disGetNextSymbol 0xc174e LB 0x28d7 -> off=0x0 cb=00000000000000a5 uValue=00000000000c174e 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc174e LB 0xa5
    push bp                                   ; 55                          ; 0xc174e vgabios.c:1126
    mov bp, sp                                ; 89 e5                       ; 0xc174f
    push si                                   ; 56                          ; 0xc1751
    push di                                   ; 57                          ; 0xc1752
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1753
    mov dh, bl                                ; 88 de                       ; 0xc1756
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1758
    movzx di, dl                              ; 0f b6 fa                    ; 0xc175b vgabios.c:1132
    movzx si, byte [bp+006h]                  ; 0f b6 76 06                 ; 0xc175e
    imul di, si                               ; 0f af fe                    ; 0xc1762
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1765
    imul di, bx                               ; 0f af fb                    ; 0xc1769
    sar di, 1                                 ; d1 ff                       ; 0xc176c
    xor ah, ah                                ; 30 e4                       ; 0xc176e
    add di, ax                                ; 01 c7                       ; 0xc1770
    mov word [bp-00ch], di                    ; 89 7e f4                    ; 0xc1772
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1775 vgabios.c:1133
    imul dx, si                               ; 0f af d6                    ; 0xc1778
    imul dx, bx                               ; 0f af d3                    ; 0xc177b
    sar dx, 1                                 ; d1 fa                       ; 0xc177e
    add dx, ax                                ; 01 c2                       ; 0xc1780
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc1782
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc1785 vgabios.c:1134
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1788
    cwd                                       ; 99                          ; 0xc178c
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc178d
    sar ax, 1                                 ; d1 f8                       ; 0xc178f
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc1791
    cmp bx, ax                                ; 39 c3                       ; 0xc1795
    jnl short 017eah                          ; 7d 51                       ; 0xc1797
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1799 vgabios.c:1136
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc179d
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc17a0
    imul bx, ax                               ; 0f af d8                    ; 0xc17a4
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc17a7
    add si, bx                                ; 01 de                       ; 0xc17aa
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17ac
    add di, bx                                ; 01 df                       ; 0xc17af
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc17b1
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc17b4
    mov es, dx                                ; 8e c2                       ; 0xc17b7
    cld                                       ; fc                          ; 0xc17b9
    jcxz 017c2h                               ; e3 06                       ; 0xc17ba
    push DS                                   ; 1e                          ; 0xc17bc
    mov ds, dx                                ; 8e da                       ; 0xc17bd
    rep movsb                                 ; f3 a4                       ; 0xc17bf
    pop DS                                    ; 1f                          ; 0xc17c1
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc17c2 vgabios.c:1137
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc17c5
    add si, bx                                ; 01 de                       ; 0xc17c9
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17cb
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc17ce
    add di, bx                                ; 01 df                       ; 0xc17d2
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc17d4
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc17d7
    mov es, dx                                ; 8e c2                       ; 0xc17da
    cld                                       ; fc                          ; 0xc17dc
    jcxz 017e5h                               ; e3 06                       ; 0xc17dd
    push DS                                   ; 1e                          ; 0xc17df
    mov ds, dx                                ; 8e da                       ; 0xc17e0
    rep movsb                                 ; f3 a4                       ; 0xc17e2
    pop DS                                    ; 1f                          ; 0xc17e4
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc17e5 vgabios.c:1138
    jmp short 01788h                          ; eb 9e                       ; 0xc17e8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17ea vgabios.c:1139
    pop di                                    ; 5f                          ; 0xc17ed
    pop si                                    ; 5e                          ; 0xc17ee
    pop bp                                    ; 5d                          ; 0xc17ef
    retn 00004h                               ; c2 04 00                    ; 0xc17f0
  ; disGetNextSymbol 0xc17f3 LB 0x2832 -> off=0x0 cb=0000000000000083 uValue=00000000000c17f3 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc17f3 LB 0x83
    push bp                                   ; 55                          ; 0xc17f3 vgabios.c:1142
    mov bp, sp                                ; 89 e5                       ; 0xc17f4
    push si                                   ; 56                          ; 0xc17f6
    push di                                   ; 57                          ; 0xc17f7
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc17f8
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc17fb
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc17fe
    movzx bx, dl                              ; 0f b6 da                    ; 0xc1801 vgabios.c:1148
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1804
    imul bx, dx                               ; 0f af da                    ; 0xc1808
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc180b
    imul dx, bx                               ; 0f af d3                    ; 0xc180e
    sar dx, 1                                 ; d1 fa                       ; 0xc1811
    xor ah, ah                                ; 30 e4                       ; 0xc1813
    add dx, ax                                ; 01 c2                       ; 0xc1815
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc1817
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc181a vgabios.c:1149
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc181d
    cwd                                       ; 99                          ; 0xc1821
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1822
    sar ax, 1                                 ; d1 f8                       ; 0xc1824
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc1826
    cmp dx, ax                                ; 39 c2                       ; 0xc182a
    jnl short 0186dh                          ; 7d 3f                       ; 0xc182c
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc182e vgabios.c:1151
    movzx bx, byte [bp+006h]                  ; 0f b6 5e 06                 ; 0xc1832
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1836
    imul dx, ax                               ; 0f af d0                    ; 0xc183a
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc183d
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1840
    add di, dx                                ; 01 d7                       ; 0xc1843
    mov cx, si                                ; 89 f1                       ; 0xc1845
    mov ax, bx                                ; 89 d8                       ; 0xc1847
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1849
    mov es, dx                                ; 8e c2                       ; 0xc184c
    cld                                       ; fc                          ; 0xc184e
    jcxz 01853h                               ; e3 02                       ; 0xc184f
    rep stosb                                 ; f3 aa                       ; 0xc1851
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1853 vgabios.c:1152
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1856
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc185a
    mov cx, si                                ; 89 f1                       ; 0xc185d
    mov ax, bx                                ; 89 d8                       ; 0xc185f
    mov es, dx                                ; 8e c2                       ; 0xc1861
    cld                                       ; fc                          ; 0xc1863
    jcxz 01868h                               ; e3 02                       ; 0xc1864
    rep stosb                                 ; f3 aa                       ; 0xc1866
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1868 vgabios.c:1153
    jmp short 0181dh                          ; eb b0                       ; 0xc186b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc186d vgabios.c:1154
    pop di                                    ; 5f                          ; 0xc1870
    pop si                                    ; 5e                          ; 0xc1871
    pop bp                                    ; 5d                          ; 0xc1872
    retn 00004h                               ; c2 04 00                    ; 0xc1873
  ; disGetNextSymbol 0xc1876 LB 0x27af -> off=0x0 cb=000000000000007a uValue=00000000000c1876 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1876 LB 0x7a
    push bp                                   ; 55                          ; 0xc1876 vgabios.c:1157
    mov bp, sp                                ; 89 e5                       ; 0xc1877
    push si                                   ; 56                          ; 0xc1879
    push di                                   ; 57                          ; 0xc187a
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc187b
    mov ah, al                                ; 88 c4                       ; 0xc187e
    mov al, bl                                ; 88 d8                       ; 0xc1880
    mov bx, cx                                ; 89 cb                       ; 0xc1882
    xor dh, dh                                ; 30 f6                       ; 0xc1884 vgabios.c:1163
    movzx di, byte [bp+006h]                  ; 0f b6 7e 06                 ; 0xc1886
    imul dx, di                               ; 0f af d7                    ; 0xc188a
    imul dx, word [bp+004h]                   ; 0f af 56 04                 ; 0xc188d
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1891
    add dx, si                                ; 01 f2                       ; 0xc1894
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1896
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc1899
    xor ah, ah                                ; 30 e4                       ; 0xc189c vgabios.c:1164
    imul ax, di                               ; 0f af c7                    ; 0xc189e
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc18a1
    add si, ax                                ; 01 c6                       ; 0xc18a5
    sal si, 003h                              ; c1 e6 03                    ; 0xc18a7
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc18aa
    sal bx, 003h                              ; c1 e3 03                    ; 0xc18ad vgabios.c:1165
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc18b0 vgabios.c:1166
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc18b4 vgabios.c:1167
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18b8
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc18bb
    jnc short 018e7h                          ; 73 27                       ; 0xc18be
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc18c0 vgabios.c:1169
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc18c4
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc18c8
    add si, ax                                ; 01 c6                       ; 0xc18cb
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc18cd
    add di, ax                                ; 01 c7                       ; 0xc18d0
    mov cx, bx                                ; 89 d9                       ; 0xc18d2
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc18d4
    mov es, dx                                ; 8e c2                       ; 0xc18d7
    cld                                       ; fc                          ; 0xc18d9
    jcxz 018e2h                               ; e3 06                       ; 0xc18da
    push DS                                   ; 1e                          ; 0xc18dc
    mov ds, dx                                ; 8e da                       ; 0xc18dd
    rep movsb                                 ; f3 a4                       ; 0xc18df
    pop DS                                    ; 1f                          ; 0xc18e1
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18e2 vgabios.c:1170
    jmp short 018b8h                          ; eb d1                       ; 0xc18e5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18e7 vgabios.c:1171
    pop di                                    ; 5f                          ; 0xc18ea
    pop si                                    ; 5e                          ; 0xc18eb
    pop bp                                    ; 5d                          ; 0xc18ec
    retn 00004h                               ; c2 04 00                    ; 0xc18ed
  ; disGetNextSymbol 0xc18f0 LB 0x2735 -> off=0x0 cb=000000000000005d uValue=00000000000c18f0 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc18f0 LB 0x5d
    push bp                                   ; 55                          ; 0xc18f0 vgabios.c:1174
    mov bp, sp                                ; 89 e5                       ; 0xc18f1
    push si                                   ; 56                          ; 0xc18f3
    push di                                   ; 57                          ; 0xc18f4
    push ax                                   ; 50                          ; 0xc18f5
    push ax                                   ; 50                          ; 0xc18f6
    mov si, bx                                ; 89 de                       ; 0xc18f7
    mov bx, cx                                ; 89 cb                       ; 0xc18f9
    xor dh, dh                                ; 30 f6                       ; 0xc18fb vgabios.c:1180
    movzx di, byte [bp+004h]                  ; 0f b6 7e 04                 ; 0xc18fd
    imul dx, di                               ; 0f af d7                    ; 0xc1901
    imul dx, cx                               ; 0f af d1                    ; 0xc1904
    xor ah, ah                                ; 30 e4                       ; 0xc1907
    add ax, dx                                ; 01 d0                       ; 0xc1909
    sal ax, 003h                              ; c1 e0 03                    ; 0xc190b
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc190e
    sal si, 003h                              ; c1 e6 03                    ; 0xc1911 vgabios.c:1181
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1914 vgabios.c:1182
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1917 vgabios.c:1183
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc191b
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc191e
    jnc short 01944h                          ; 73 21                       ; 0xc1921
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1923 vgabios.c:1185
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc1927
    imul dx, bx                               ; 0f af d3                    ; 0xc192b
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc192e
    add di, dx                                ; 01 d7                       ; 0xc1931
    mov cx, si                                ; 89 f1                       ; 0xc1933
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1935
    mov es, dx                                ; 8e c2                       ; 0xc1938
    cld                                       ; fc                          ; 0xc193a
    jcxz 0193fh                               ; e3 02                       ; 0xc193b
    rep stosb                                 ; f3 aa                       ; 0xc193d
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc193f vgabios.c:1186
    jmp short 0191bh                          ; eb d7                       ; 0xc1942
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1944 vgabios.c:1187
    pop di                                    ; 5f                          ; 0xc1947
    pop si                                    ; 5e                          ; 0xc1948
    pop bp                                    ; 5d                          ; 0xc1949
    retn 00004h                               ; c2 04 00                    ; 0xc194a
  ; disGetNextSymbol 0xc194d LB 0x26d8 -> off=0x0 cb=0000000000000630 uValue=00000000000c194d 'biosfn_scroll'
biosfn_scroll:                               ; 0xc194d LB 0x630
    push bp                                   ; 55                          ; 0xc194d vgabios.c:1190
    mov bp, sp                                ; 89 e5                       ; 0xc194e
    push si                                   ; 56                          ; 0xc1950
    push di                                   ; 57                          ; 0xc1951
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1952
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1955
    mov byte [bp-012h], dl                    ; 88 56 ee                    ; 0xc1958
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc195b
    mov byte [bp-010h], cl                    ; 88 4e f0                    ; 0xc195e
    mov dh, byte [bp+006h]                    ; 8a 76 06                    ; 0xc1961
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1964 vgabios.c:1199
    jnbe near 01f74h                          ; 0f 87 09 06                 ; 0xc1967
    cmp dh, cl                                ; 38 ce                       ; 0xc196b vgabios.c:1200
    jc near 01f74h                            ; 0f 82 03 06                 ; 0xc196d
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1971 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1974
    mov es, ax                                ; 8e c0                       ; 0xc1977
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1979
    xor ah, ah                                ; 30 e4                       ; 0xc197c vgabios.c:1204
    call 033c0h                               ; e8 3f 1a                    ; 0xc197e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1981
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1984 vgabios.c:1205
    je near 01f74h                            ; 0f 84 ea 05                 ; 0xc1986
    mov bx, 00084h                            ; bb 84 00                    ; 0xc198a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc198d
    mov es, ax                                ; 8e c0                       ; 0xc1990
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1992
    movzx cx, al                              ; 0f b6 c8                    ; 0xc1995 vgabios.c:38
    inc cx                                    ; 41                          ; 0xc1998
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1999 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc199c
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc199f vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc19a2 vgabios.c:1212
    jne short 019b1h                          ; 75 09                       ; 0xc19a6
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc19a8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc19ab
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc19ae vgabios.c:38
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc19b1 vgabios.c:1215
    cmp ax, cx                                ; 39 c8                       ; 0xc19b5
    jc short 019c0h                           ; 72 07                       ; 0xc19b7
    mov al, cl                                ; 88 c8                       ; 0xc19b9
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc19bb
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc19bd
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc19c0 vgabios.c:1216
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc19c3
    jc short 019cdh                           ; 72 05                       ; 0xc19c6
    mov dh, byte [bp-014h]                    ; 8a 76 ec                    ; 0xc19c8
    db  0feh, 0ceh
    ; dec dh                                    ; fe ce                     ; 0xc19cb
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc19cd vgabios.c:1217
    cmp ax, cx                                ; 39 c8                       ; 0xc19d1
    jbe short 019d9h                          ; 76 04                       ; 0xc19d3
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc19d5
    mov al, dh                                ; 88 f0                       ; 0xc19d9 vgabios.c:1218
    sub al, byte [bp-010h]                    ; 2a 46 f0                    ; 0xc19db
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc19de
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc19e0
    movzx di, byte [bp-006h]                  ; 0f b6 7e fa                 ; 0xc19e3 vgabios.c:1220
    mov bx, di                                ; 89 fb                       ; 0xc19e7
    sal bx, 003h                              ; c1 e3 03                    ; 0xc19e9
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc19ec
    dec ax                                    ; 48                          ; 0xc19ef
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc19f0
    mov ax, cx                                ; 89 c8                       ; 0xc19f3
    dec ax                                    ; 48                          ; 0xc19f5
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc19f6
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc19f9
    imul ax, cx                               ; 0f af c1                    ; 0xc19fc
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc19ff
    jne near 01ba8h                           ; 0f 85 a0 01                 ; 0xc1a04
    mov cx, ax                                ; 89 c1                       ; 0xc1a08 vgabios.c:1223
    add cx, ax                                ; 01 c1                       ; 0xc1a0a
    or cl, 0ffh                               ; 80 c9 ff                    ; 0xc1a0c
    movzx si, byte [bp+008h]                  ; 0f b6 76 08                 ; 0xc1a0f
    inc cx                                    ; 41                          ; 0xc1a13
    imul cx, si                               ; 0f af ce                    ; 0xc1a14
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc1a17
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1a1a vgabios.c:1228
    jne short 01a5ch                          ; 75 3c                       ; 0xc1a1e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1a20
    jne short 01a5ch                          ; 75 36                       ; 0xc1a24
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1a26
    jne short 01a5ch                          ; 75 30                       ; 0xc1a2a
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1a2c
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1a30
    jne short 01a5ch                          ; 75 27                       ; 0xc1a33
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1a35
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1a38
    jne short 01a5ch                          ; 75 1f                       ; 0xc1a3b
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1a3d vgabios.c:1230
    sal dx, 008h                              ; c1 e2 08                    ; 0xc1a41
    add dx, strict byte 00020h                ; 83 c2 20                    ; 0xc1a44
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1a47
    mov cx, ax                                ; 89 c1                       ; 0xc1a4b
    mov ax, dx                                ; 89 d0                       ; 0xc1a4d
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1a4f
    mov es, bx                                ; 8e c3                       ; 0xc1a52
    cld                                       ; fc                          ; 0xc1a54
    jcxz 01a59h                               ; e3 02                       ; 0xc1a55
    rep stosw                                 ; f3 ab                       ; 0xc1a57
    jmp near 01f74h                           ; e9 18 05                    ; 0xc1a59 vgabios.c:1232
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1a5c vgabios.c:1234
    jne near 01afbh                           ; 0f 85 97 00                 ; 0xc1a60
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1a64 vgabios.c:1235
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1a68
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1a6b
    cmp dx, word [bp-01ah]                    ; 3b 56 e6                    ; 0xc1a6f
    jc near 01f74h                            ; 0f 82 fe 04                 ; 0xc1a72
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1a76 vgabios.c:1237
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1a7a
    cmp ax, dx                                ; 39 d0                       ; 0xc1a7d
    jnbe short 01a87h                         ; 77 06                       ; 0xc1a7f
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1a81
    jne short 01abbh                          ; 75 34                       ; 0xc1a85
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1a87 vgabios.c:1238
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1a8b
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1a8f
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1a92
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1a95
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1a98
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1a9c
    add dx, bx                                ; 01 da                       ; 0xc1aa0
    add dx, dx                                ; 01 d2                       ; 0xc1aa2
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1aa4
    add di, dx                                ; 01 d7                       ; 0xc1aa7
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1aa9
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1aad
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1ab0
    cld                                       ; fc                          ; 0xc1ab4
    jcxz 01ab9h                               ; e3 02                       ; 0xc1ab5
    rep stosw                                 ; f3 ab                       ; 0xc1ab7
    jmp short 01af5h                          ; eb 3a                       ; 0xc1ab9 vgabios.c:1239
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1abb vgabios.c:1240
    mov si, ax                                ; 89 c6                       ; 0xc1abf
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1ac1
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1ac5
    add si, dx                                ; 01 d6                       ; 0xc1ac9
    add si, si                                ; 01 f6                       ; 0xc1acb
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1acd
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1ad1
    mov ax, word [bx+047b2h]                  ; 8b 87 b2 47                 ; 0xc1ad4
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1ad8
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1adb
    mov di, dx                                ; 89 d7                       ; 0xc1adf
    add di, bx                                ; 01 df                       ; 0xc1ae1
    add di, di                                ; 01 ff                       ; 0xc1ae3
    add di, word [bp-01ch]                    ; 03 7e e4                    ; 0xc1ae5
    mov dx, ax                                ; 89 c2                       ; 0xc1ae8
    mov es, ax                                ; 8e c0                       ; 0xc1aea
    cld                                       ; fc                          ; 0xc1aec
    jcxz 01af5h                               ; e3 06                       ; 0xc1aed
    push DS                                   ; 1e                          ; 0xc1aef
    mov ds, dx                                ; 8e da                       ; 0xc1af0
    rep movsw                                 ; f3 a5                       ; 0xc1af2
    pop DS                                    ; 1f                          ; 0xc1af4
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1af5 vgabios.c:1241
    jmp near 01a6bh                           ; e9 70 ff                    ; 0xc1af8
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1afb vgabios.c:1244
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1aff
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1b02
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1b06
    jnbe near 01f74h                          ; 0f 87 67 04                 ; 0xc1b09
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1b0d vgabios.c:1246
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1b11
    add ax, dx                                ; 01 d0                       ; 0xc1b15
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1b17
    jnbe short 01b22h                         ; 77 06                       ; 0xc1b1a
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b1c
    jne short 01b56h                          ; 75 34                       ; 0xc1b20
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1b22 vgabios.c:1247
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1b26
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1b2a
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b2d
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc1b30
    imul dx, word [bp-014h]                   ; 0f af 56 ec                 ; 0xc1b33
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1b37
    add dx, bx                                ; 01 da                       ; 0xc1b3b
    add dx, dx                                ; 01 d2                       ; 0xc1b3d
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b3f
    add di, dx                                ; 01 d7                       ; 0xc1b42
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1b44
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b48
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b4b
    cld                                       ; fc                          ; 0xc1b4f
    jcxz 01b54h                               ; e3 02                       ; 0xc1b50
    rep stosw                                 ; f3 ab                       ; 0xc1b52
    jmp short 01b97h                          ; eb 41                       ; 0xc1b54 vgabios.c:1248
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1b56 vgabios.c:1249
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1b5a
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1b5e
    sub si, ax                                ; 29 c6                       ; 0xc1b61
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1b63
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1b67
    add si, dx                                ; 01 d6                       ; 0xc1b6b
    add si, si                                ; 01 f6                       ; 0xc1b6d
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1b6f
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b73
    mov ax, word [bx+047b2h]                  ; 8b 87 b2 47                 ; 0xc1b76
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1b7a
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1b7d
    add dx, bx                                ; 01 da                       ; 0xc1b81
    add dx, dx                                ; 01 d2                       ; 0xc1b83
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b85
    add di, dx                                ; 01 d7                       ; 0xc1b88
    mov dx, ax                                ; 89 c2                       ; 0xc1b8a
    mov es, ax                                ; 8e c0                       ; 0xc1b8c
    cld                                       ; fc                          ; 0xc1b8e
    jcxz 01b97h                               ; e3 06                       ; 0xc1b8f
    push DS                                   ; 1e                          ; 0xc1b91
    mov ds, dx                                ; 8e da                       ; 0xc1b92
    rep movsw                                 ; f3 a5                       ; 0xc1b94
    pop DS                                    ; 1f                          ; 0xc1b96
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1b97 vgabios.c:1250
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1b9b
    jc near 01f74h                            ; 0f 82 d2 03                 ; 0xc1b9e
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1ba2 vgabios.c:1251
    jmp near 01b02h                           ; e9 5a ff                    ; 0xc1ba5
    movzx di, byte [di+0482eh]                ; 0f b6 bd 2e 48              ; 0xc1ba8 vgabios.c:1257
    sal di, 006h                              ; c1 e7 06                    ; 0xc1bad
    mov dl, byte [di+04844h]                  ; 8a 95 44 48                 ; 0xc1bb0
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc1bb4
    mov dl, byte [bx+047b0h]                  ; 8a 97 b0 47                 ; 0xc1bb7 vgabios.c:1258
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc1bbb
    jc short 01bd1h                           ; 72 11                       ; 0xc1bbe
    jbe short 01bdbh                          ; 76 19                       ; 0xc1bc0
    cmp dl, 005h                              ; 80 fa 05                    ; 0xc1bc2
    je near 01e56h                            ; 0f 84 8d 02                 ; 0xc1bc5
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc1bc9
    je short 01bdbh                           ; 74 0d                       ; 0xc1bcc
    jmp near 01f74h                           ; e9 a3 03                    ; 0xc1bce
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1bd1
    je near 01d1bh                            ; 0f 84 43 01                 ; 0xc1bd4
    jmp near 01f74h                           ; e9 99 03                    ; 0xc1bd8
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1bdb vgabios.c:1262
    jne short 01c34h                          ; 75 53                       ; 0xc1bdf
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1be1
    jne short 01c34h                          ; 75 4d                       ; 0xc1be5
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1be7
    jne short 01c34h                          ; 75 47                       ; 0xc1beb
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1bed
    mov ax, cx                                ; 89 c8                       ; 0xc1bf1
    dec ax                                    ; 48                          ; 0xc1bf3
    cmp bx, ax                                ; 39 c3                       ; 0xc1bf4
    jne short 01c34h                          ; 75 3c                       ; 0xc1bf6
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc1bf8
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc1bfb
    dec dx                                    ; 4a                          ; 0xc1bfe
    cmp ax, dx                                ; 39 d0                       ; 0xc1bff
    jne short 01c34h                          ; 75 31                       ; 0xc1c01
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1c03 vgabios.c:1264
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1c06
    out DX, ax                                ; ef                          ; 0xc1c09
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1c0a vgabios.c:1265
    imul ax, cx                               ; 0f af c1                    ; 0xc1c0d
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1c10
    imul cx, ax                               ; 0f af c8                    ; 0xc1c14
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1c17
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1c1b
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c1f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1c22
    xor di, di                                ; 31 ff                       ; 0xc1c26
    cld                                       ; fc                          ; 0xc1c28
    jcxz 01c2dh                               ; e3 02                       ; 0xc1c29
    rep stosb                                 ; f3 aa                       ; 0xc1c2b
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1c2d vgabios.c:1266
    out DX, ax                                ; ef                          ; 0xc1c30
    jmp near 01f74h                           ; e9 40 03                    ; 0xc1c31 vgabios.c:1268
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1c34 vgabios.c:1270
    jne short 01ca3h                          ; 75 69                       ; 0xc1c38
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1c3a vgabios.c:1271
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1c3e
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1c41
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1c45
    jc near 01f74h                            ; 0f 82 28 03                 ; 0xc1c48
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1c4c vgabios.c:1273
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1c50
    cmp dx, ax                                ; 39 c2                       ; 0xc1c53
    jnbe short 01c5dh                         ; 77 06                       ; 0xc1c55
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1c57
    jne short 01c7ch                          ; 75 1f                       ; 0xc1c5b
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1c5d vgabios.c:1274
    push ax                                   ; 50                          ; 0xc1c61
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1c62
    push ax                                   ; 50                          ; 0xc1c66
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1c67
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1c6b
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1c6f
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1c73
    call 016edh                               ; e8 73 fa                    ; 0xc1c77
    jmp short 01c9eh                          ; eb 22                       ; 0xc1c7a vgabios.c:1275
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1c7c vgabios.c:1276
    push ax                                   ; 50                          ; 0xc1c80
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1c81
    push ax                                   ; 50                          ; 0xc1c85
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1c86
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1c8a
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1c8e
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1c91
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1c94
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1c97
    call 01677h                               ; e8 d9 f9                    ; 0xc1c9b
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1c9e vgabios.c:1277
    jmp short 01c41h                          ; eb 9e                       ; 0xc1ca1
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1ca3 vgabios.c:1280
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1ca7
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1caa
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1cae
    jnbe near 01f74h                          ; 0f 87 bf 02                 ; 0xc1cb1
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1cb5 vgabios.c:1282
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1cb9
    add ax, dx                                ; 01 d0                       ; 0xc1cbd
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1cbf
    jnbe short 01ccah                         ; 77 06                       ; 0xc1cc2
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1cc4
    jne short 01ce9h                          ; 75 1f                       ; 0xc1cc8
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1cca vgabios.c:1283
    push ax                                   ; 50                          ; 0xc1cce
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ccf
    push ax                                   ; 50                          ; 0xc1cd3
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1cd4
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1cd8
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1cdc
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ce0
    call 016edh                               ; e8 06 fa                    ; 0xc1ce4
    jmp short 01d0bh                          ; eb 22                       ; 0xc1ce7 vgabios.c:1284
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ce9 vgabios.c:1285
    push ax                                   ; 50                          ; 0xc1ced
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1cee
    push ax                                   ; 50                          ; 0xc1cf2
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1cf3
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1cf7
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1cfb
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1cfe
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1d01
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1d04
    call 01677h                               ; e8 6c f9                    ; 0xc1d08
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1d0b vgabios.c:1286
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1d0f
    jc near 01f74h                            ; 0f 82 5e 02                 ; 0xc1d12
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1d16 vgabios.c:1287
    jmp short 01caah                          ; eb 8f                       ; 0xc1d19
    mov dl, byte [bx+047b1h]                  ; 8a 97 b1 47                 ; 0xc1d1b vgabios.c:1292
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d1f vgabios.c:1293
    jne short 01d61h                          ; 75 3c                       ; 0xc1d23
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d25
    jne short 01d61h                          ; 75 36                       ; 0xc1d29
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1d2b
    jne short 01d61h                          ; 75 30                       ; 0xc1d2f
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1d31
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1d35
    jne short 01d61h                          ; 75 27                       ; 0xc1d38
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc1d3a
    cmp cx, word [bp-018h]                    ; 3b 4e e8                    ; 0xc1d3d
    jne short 01d61h                          ; 75 1f                       ; 0xc1d40
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1d42 vgabios.c:1295
    imul ax, cx                               ; 0f af c1                    ; 0xc1d46
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc1d49
    imul cx, ax                               ; 0f af c8                    ; 0xc1d4c
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1d4f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1d53
    xor di, di                                ; 31 ff                       ; 0xc1d57
    cld                                       ; fc                          ; 0xc1d59
    jcxz 01d5eh                               ; e3 02                       ; 0xc1d5a
    rep stosb                                 ; f3 aa                       ; 0xc1d5c
    jmp near 01f74h                           ; e9 13 02                    ; 0xc1d5e vgabios.c:1297
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1d61 vgabios.c:1299
    jne short 01d6fh                          ; 75 09                       ; 0xc1d64
    sal byte [bp-010h], 1                     ; d0 66 f0                    ; 0xc1d66 vgabios.c:1301
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1d69 vgabios.c:1302
    sal word [bp-014h], 1                     ; d1 66 ec                    ; 0xc1d6c vgabios.c:1303
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d6f vgabios.c:1306
    jne short 01ddeh                          ; 75 69                       ; 0xc1d73
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1d75 vgabios.c:1307
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1d79
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1d7c
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1d80
    jc near 01f74h                            ; 0f 82 ed 01                 ; 0xc1d83
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1d87 vgabios.c:1309
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1d8b
    cmp dx, ax                                ; 39 c2                       ; 0xc1d8e
    jnbe short 01d98h                         ; 77 06                       ; 0xc1d90
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d92
    jne short 01db7h                          ; 75 1f                       ; 0xc1d96
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1d98 vgabios.c:1310
    push ax                                   ; 50                          ; 0xc1d9c
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1d9d
    push ax                                   ; 50                          ; 0xc1da1
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1da2
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1da6
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1daa
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1dae
    call 017f3h                               ; e8 3e fa                    ; 0xc1db2
    jmp short 01dd9h                          ; eb 22                       ; 0xc1db5 vgabios.c:1311
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1db7 vgabios.c:1312
    push ax                                   ; 50                          ; 0xc1dbb
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1dbc
    push ax                                   ; 50                          ; 0xc1dc0
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1dc1
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1dc5
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1dc9
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1dcc
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1dcf
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1dd2
    call 0174eh                               ; e8 75 f9                    ; 0xc1dd6
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1dd9 vgabios.c:1313
    jmp short 01d7ch                          ; eb 9e                       ; 0xc1ddc
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1dde vgabios.c:1316
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1de2
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1de5
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1de9
    jnbe near 01f74h                          ; 0f 87 84 01                 ; 0xc1dec
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1df0 vgabios.c:1318
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1df4
    add ax, dx                                ; 01 d0                       ; 0xc1df8
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1dfa
    jnbe short 01e05h                         ; 77 06                       ; 0xc1dfd
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1dff
    jne short 01e24h                          ; 75 1f                       ; 0xc1e03
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1e05 vgabios.c:1319
    push ax                                   ; 50                          ; 0xc1e09
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e0a
    push ax                                   ; 50                          ; 0xc1e0e
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1e0f
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1e13
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1e17
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e1b
    call 017f3h                               ; e8 d1 f9                    ; 0xc1e1f
    jmp short 01e46h                          ; eb 22                       ; 0xc1e22 vgabios.c:1320
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e24 vgabios.c:1321
    push ax                                   ; 50                          ; 0xc1e28
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1e29
    push ax                                   ; 50                          ; 0xc1e2d
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e2e
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1e32
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1e36
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1e39
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1e3c
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e3f
    call 0174eh                               ; e8 08 f9                    ; 0xc1e43
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1e46 vgabios.c:1322
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1e4a
    jc near 01f74h                            ; 0f 82 23 01                 ; 0xc1e4d
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1e51 vgabios.c:1323
    jmp short 01de5h                          ; eb 8f                       ; 0xc1e54
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1e56 vgabios.c:1328
    jne short 01e97h                          ; 75 3b                       ; 0xc1e5a
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1e5c
    jne short 01e97h                          ; 75 35                       ; 0xc1e60
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1e62
    jne short 01e97h                          ; 75 2f                       ; 0xc1e66
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1e68
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1e6c
    jne short 01e97h                          ; 75 26                       ; 0xc1e6f
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1e71
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1e74
    jne short 01e97h                          ; 75 1e                       ; 0xc1e77
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2                 ; 0xc1e79 vgabios.c:1330
    mov cx, ax                                ; 89 c1                       ; 0xc1e7d
    imul cx, dx                               ; 0f af ca                    ; 0xc1e7f
    sal cx, 003h                              ; c1 e1 03                    ; 0xc1e82
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1e85
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1e89
    xor di, di                                ; 31 ff                       ; 0xc1e8d
    cld                                       ; fc                          ; 0xc1e8f
    jcxz 01e94h                               ; e3 02                       ; 0xc1e90
    rep stosb                                 ; f3 aa                       ; 0xc1e92
    jmp near 01f74h                           ; e9 dd 00                    ; 0xc1e94 vgabios.c:1332
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1e97 vgabios.c:1335
    jne short 01f03h                          ; 75 66                       ; 0xc1e9b
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1e9d vgabios.c:1336
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1ea1
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1ea4
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1ea8
    jc near 01f74h                            ; 0f 82 c5 00                 ; 0xc1eab
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1eaf vgabios.c:1338
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1eb3
    cmp dx, ax                                ; 39 c2                       ; 0xc1eb6
    jnbe short 01ec0h                         ; 77 06                       ; 0xc1eb8
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1eba
    jne short 01edeh                          ; 75 1e                       ; 0xc1ebe
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1ec0 vgabios.c:1339
    push ax                                   ; 50                          ; 0xc1ec4
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ec5
    push ax                                   ; 50                          ; 0xc1ec9
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1eca
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1ece
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ed2
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1ed6
    call 018f0h                               ; e8 14 fa                    ; 0xc1ed9
    jmp short 01efeh                          ; eb 20                       ; 0xc1edc vgabios.c:1340
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ede vgabios.c:1341
    push ax                                   ; 50                          ; 0xc1ee2
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc1ee3
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1ee6
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1eea
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1eee
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1ef1
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1ef4
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ef7
    call 01876h                               ; e8 78 f9                    ; 0xc1efb
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1efe vgabios.c:1342
    jmp short 01ea4h                          ; eb a1                       ; 0xc1f01
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1f03 vgabios.c:1345
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1f07
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1f0a
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f0e
    jnbe short 01f74h                         ; 77 61                       ; 0xc1f11
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1f13 vgabios.c:1347
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1f17
    add ax, dx                                ; 01 d0                       ; 0xc1f1b
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f1d
    jnbe short 01f28h                         ; 77 06                       ; 0xc1f20
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f22
    jne short 01f46h                          ; 75 1e                       ; 0xc1f26
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1f28 vgabios.c:1348
    push ax                                   ; 50                          ; 0xc1f2c
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f2d
    push ax                                   ; 50                          ; 0xc1f31
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1f32
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1f36
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1f3a
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1f3e
    call 018f0h                               ; e8 ac f9                    ; 0xc1f41
    jmp short 01f66h                          ; eb 20                       ; 0xc1f44 vgabios.c:1349
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f46 vgabios.c:1350
    push ax                                   ; 50                          ; 0xc1f4a
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc1f4b
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1f4e
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1f52
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1f56
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1f59
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1f5c
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1f5f
    call 01876h                               ; e8 10 f9                    ; 0xc1f63
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1f66 vgabios.c:1351
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f6a
    jc short 01f74h                           ; 72 05                       ; 0xc1f6d
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1f6f vgabios.c:1352
    jmp short 01f0ah                          ; eb 96                       ; 0xc1f72
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1f74 vgabios.c:1363
    pop di                                    ; 5f                          ; 0xc1f77
    pop si                                    ; 5e                          ; 0xc1f78
    pop bp                                    ; 5d                          ; 0xc1f79
    retn 00008h                               ; c2 08 00                    ; 0xc1f7a
  ; disGetNextSymbol 0xc1f7d LB 0x20a8 -> off=0x0 cb=00000000000000ff uValue=00000000000c1f7d 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc1f7d LB 0xff
    push bp                                   ; 55                          ; 0xc1f7d vgabios.c:1366
    mov bp, sp                                ; 89 e5                       ; 0xc1f7e
    push si                                   ; 56                          ; 0xc1f80
    push di                                   ; 57                          ; 0xc1f81
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1f82
    mov ah, al                                ; 88 c4                       ; 0xc1f85
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1f87
    mov al, bl                                ; 88 d8                       ; 0xc1f8a
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc1f8c vgabios.c:57
    xor si, si                                ; 31 f6                       ; 0xc1f8f
    mov es, si                                ; 8e c6                       ; 0xc1f91
    mov si, word [es:bx]                      ; 26 8b 37                    ; 0xc1f93
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc1f96
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1f9a vgabios.c:58
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc1f9d
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc1fa0 vgabios.c:1375
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1fa3
    imul bx, cx                               ; 0f af d9                    ; 0xc1fa7
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc1faa
    imul si, bx                               ; 0f af f3                    ; 0xc1fae
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1fb1
    add si, bx                                ; 01 de                       ; 0xc1fb4
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc1fb6 vgabios.c:47
    mov di, strict word 00040h                ; bf 40 00                    ; 0xc1fb9
    mov es, di                                ; 8e c7                       ; 0xc1fbc
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1fbe
    movzx di, byte [bp+008h]                  ; 0f b6 7e 08                 ; 0xc1fc1 vgabios.c:48
    imul bx, di                               ; 0f af df                    ; 0xc1fc5
    add si, bx                                ; 01 de                       ; 0xc1fc8
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc1fca vgabios.c:1377
    imul ax, cx                               ; 0f af c1                    ; 0xc1fcd
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1fd0
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc1fd3 vgabios.c:1378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1fd6
    out DX, ax                                ; ef                          ; 0xc1fd9
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1fda vgabios.c:1379
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1fdd
    out DX, ax                                ; ef                          ; 0xc1fe0
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1fe1 vgabios.c:1380
    je short 01fedh                           ; 74 06                       ; 0xc1fe5
    mov ax, 01803h                            ; b8 03 18                    ; 0xc1fe7 vgabios.c:1382
    out DX, ax                                ; ef                          ; 0xc1fea
    jmp short 01ff1h                          ; eb 04                       ; 0xc1feb vgabios.c:1384
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1fed vgabios.c:1386
    out DX, ax                                ; ef                          ; 0xc1ff0
    xor ch, ch                                ; 30 ed                       ; 0xc1ff1 vgabios.c:1388
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc1ff3
    jnc short 02064h                          ; 73 6c                       ; 0xc1ff6
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1ff8 vgabios.c:1390
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1ffb
    imul bx, ax                               ; 0f af d8                    ; 0xc1fff
    add bx, si                                ; 01 f3                       ; 0xc2002
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc2004 vgabios.c:1391
    jmp short 0201ch                          ; eb 12                       ; 0xc2008
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc200a vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc200d
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc200f
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2013 vgabios.c:1404
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc2016
    jnc short 02060h                          ; 73 44                       ; 0xc201a
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc201c
    mov cl, al                                ; 88 c1                       ; 0xc2020
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2022
    sar ax, CL                                ; d3 f8                       ; 0xc2025
    xor ah, ah                                ; 30 e4                       ; 0xc2027
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc2029
    sal ax, 008h                              ; c1 e0 08                    ; 0xc202c
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc202f
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2031
    out DX, ax                                ; ef                          ; 0xc2034
    mov dx, bx                                ; 89 da                       ; 0xc2035
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2037
    call 033e7h                               ; e8 aa 13                    ; 0xc203a
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc203d
    add ax, word [bp-00eh]                    ; 03 46 f2                    ; 0xc2040
    les di, [bp-00ch]                         ; c4 7e f4                    ; 0xc2043
    add di, ax                                ; 01 c7                       ; 0xc2046
    movzx ax, byte [es:di]                    ; 26 0f b6 05                 ; 0xc2048
    test word [bp-010h], ax                   ; 85 46 f0                    ; 0xc204c
    je short 0200ah                           ; 74 b9                       ; 0xc204f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2051
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc2054
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2056
    mov es, di                                ; 8e c7                       ; 0xc2059
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc205b
    jmp short 02013h                          ; eb b3                       ; 0xc205e
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2060 vgabios.c:1405
    jmp short 01ff3h                          ; eb 8f                       ; 0xc2062
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2064 vgabios.c:1406
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2067
    out DX, ax                                ; ef                          ; 0xc206a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc206b vgabios.c:1407
    out DX, ax                                ; ef                          ; 0xc206e
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc206f vgabios.c:1408
    out DX, ax                                ; ef                          ; 0xc2072
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2073 vgabios.c:1409
    pop di                                    ; 5f                          ; 0xc2076
    pop si                                    ; 5e                          ; 0xc2077
    pop bp                                    ; 5d                          ; 0xc2078
    retn 00006h                               ; c2 06 00                    ; 0xc2079
  ; disGetNextSymbol 0xc207c LB 0x1fa9 -> off=0x0 cb=00000000000000dd uValue=00000000000c207c 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc207c LB 0xdd
    push si                                   ; 56                          ; 0xc207c vgabios.c:1412
    push di                                   ; 57                          ; 0xc207d
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc207e
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc2082 vgabios.c:1419
    xor bh, bh                                ; 30 ff                       ; 0xc2085 vgabios.c:1420
    movzx si, byte [bp+00ah]                  ; 0f b6 76 0a                 ; 0xc2087
    imul si, bx                               ; 0f af f3                    ; 0xc208b
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc208e
    imul bx, bx, 00140h                       ; 69 db 40 01                 ; 0xc2091
    add si, bx                                ; 01 de                       ; 0xc2095
    mov word [bp-004h], si                    ; 89 76 fc                    ; 0xc2097
    xor ah, ah                                ; 30 e4                       ; 0xc209a vgabios.c:1421
    sal ax, 003h                              ; c1 e0 03                    ; 0xc209c
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc209f
    xor ah, ah                                ; 30 e4                       ; 0xc20a2 vgabios.c:1422
    jmp near 020c2h                           ; e9 1b 00                    ; 0xc20a4
    movzx si, ah                              ; 0f b6 f4                    ; 0xc20a7 vgabios.c:1437
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc20aa
    add si, di                                ; 01 fe                       ; 0xc20ad
    mov al, byte [si]                         ; 8a 04                       ; 0xc20af
    mov si, 0b800h                            ; be 00 b8                    ; 0xc20b1 vgabios.c:42
    mov es, si                                ; 8e c6                       ; 0xc20b4
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc20b6
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc20b9 vgabios.c:1441
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc20bb
    jnc near 02153h                           ; 0f 83 91 00                 ; 0xc20be
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc20c2
    sar bx, 1                                 ; d1 fb                       ; 0xc20c5
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc20c7
    add bx, word [bp-004h]                    ; 03 5e fc                    ; 0xc20ca
    test ah, 001h                             ; f6 c4 01                    ; 0xc20cd
    je short 020d5h                           ; 74 03                       ; 0xc20d0
    add bh, 020h                              ; 80 c7 20                    ; 0xc20d2
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc20d5
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc20d7
    jne short 020f5h                          ; 75 18                       ; 0xc20db
    test dl, dh                               ; 84 f2                       ; 0xc20dd
    je short 020a7h                           ; 74 c6                       ; 0xc20df
    mov si, 0b800h                            ; be 00 b8                    ; 0xc20e1
    mov es, si                                ; 8e c6                       ; 0xc20e4
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc20e6
    movzx si, ah                              ; 0f b6 f4                    ; 0xc20e9
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc20ec
    add si, di                                ; 01 fe                       ; 0xc20ef
    xor al, byte [si]                         ; 32 04                       ; 0xc20f1
    jmp short 020b1h                          ; eb bc                       ; 0xc20f3
    test dh, dh                               ; 84 f6                       ; 0xc20f5 vgabios.c:1443
    jbe short 020b9h                          ; 76 c0                       ; 0xc20f7
    test dl, 080h                             ; f6 c2 80                    ; 0xc20f9 vgabios.c:1445
    je short 02108h                           ; 74 0a                       ; 0xc20fc
    mov si, 0b800h                            ; be 00 b8                    ; 0xc20fe vgabios.c:37
    mov es, si                                ; 8e c6                       ; 0xc2101
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2103
    jmp short 0210ah                          ; eb 02                       ; 0xc2106 vgabios.c:1449
    xor al, al                                ; 30 c0                       ; 0xc2108 vgabios.c:1451
    mov byte [bp-002h], 000h                  ; c6 46 fe 00                 ; 0xc210a vgabios.c:1453
    jmp short 0211dh                          ; eb 0d                       ; 0xc210e
    or al, ch                                 ; 08 e8                       ; 0xc2110 vgabios.c:1463
    shr dh, 1                                 ; d0 ee                       ; 0xc2112 vgabios.c:1466
    inc byte [bp-002h]                        ; fe 46 fe                    ; 0xc2114 vgabios.c:1467
    cmp byte [bp-002h], 004h                  ; 80 7e fe 04                 ; 0xc2117
    jnc short 02148h                          ; 73 2b                       ; 0xc211b
    movzx si, ah                              ; 0f b6 f4                    ; 0xc211d
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2120
    add si, di                                ; 01 fe                       ; 0xc2123
    movzx si, byte [si]                       ; 0f b6 34                    ; 0xc2125
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc2128
    test si, cx                               ; 85 ce                       ; 0xc212b
    je short 02112h                           ; 74 e3                       ; 0xc212d
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc212f
    sub cl, byte [bp-002h]                    ; 2a 4e fe                    ; 0xc2131
    mov ch, dl                                ; 88 d5                       ; 0xc2134
    and ch, 003h                              ; 80 e5 03                    ; 0xc2136
    add cl, cl                                ; 00 c9                       ; 0xc2139
    sal ch, CL                                ; d2 e5                       ; 0xc213b
    mov cl, ch                                ; 88 e9                       ; 0xc213d
    test dl, 080h                             ; f6 c2 80                    ; 0xc213f
    je short 02110h                           ; 74 cc                       ; 0xc2142
    xor al, ch                                ; 30 e8                       ; 0xc2144
    jmp short 02112h                          ; eb ca                       ; 0xc2146
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc2148 vgabios.c:42
    mov es, cx                                ; 8e c1                       ; 0xc214b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc214d
    inc bx                                    ; 43                          ; 0xc2150 vgabios.c:1469
    jmp short 020f5h                          ; eb a2                       ; 0xc2151 vgabios.c:1470
    leave                                     ; c9                          ; 0xc2153 vgabios.c:1473
    pop di                                    ; 5f                          ; 0xc2154
    pop si                                    ; 5e                          ; 0xc2155
    retn 00004h                               ; c2 04 00                    ; 0xc2156
  ; disGetNextSymbol 0xc2159 LB 0x1ecc -> off=0x0 cb=0000000000000085 uValue=00000000000c2159 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc2159 LB 0x85
    push si                                   ; 56                          ; 0xc2159 vgabios.c:1476
    push di                                   ; 57                          ; 0xc215a
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc215b
    mov dh, dl                                ; 88 d6                       ; 0xc215f
    mov word [bp-002h], 0556ch                ; c7 46 fe 6c 55              ; 0xc2161 vgabios.c:1483
    movzx si, cl                              ; 0f b6 f1                    ; 0xc2166 vgabios.c:1484
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc2169
    imul cx, si                               ; 0f af ce                    ; 0xc216d
    sal cx, 006h                              ; c1 e1 06                    ; 0xc2170
    xor bh, bh                                ; 30 ff                       ; 0xc2173
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2175
    add bx, cx                                ; 01 cb                       ; 0xc2178
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc217a
    xor ah, ah                                ; 30 e4                       ; 0xc217d vgabios.c:1485
    mov si, ax                                ; 89 c6                       ; 0xc217f
    sal si, 003h                              ; c1 e6 03                    ; 0xc2181
    xor al, al                                ; 30 c0                       ; 0xc2184 vgabios.c:1486
    jmp short 021bdh                          ; eb 35                       ; 0xc2186
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc2188 vgabios.c:1490
    jnc short 021b7h                          ; 73 2a                       ; 0xc218b
    xor cl, cl                                ; 30 c9                       ; 0xc218d vgabios.c:1492
    movzx bx, al                              ; 0f b6 d8                    ; 0xc218f vgabios.c:1493
    add bx, si                                ; 01 f3                       ; 0xc2192
    add bx, word [bp-002h]                    ; 03 5e fe                    ; 0xc2194
    movzx bx, byte [bx]                       ; 0f b6 1f                    ; 0xc2197
    movzx di, dl                              ; 0f b6 fa                    ; 0xc219a
    test bx, di                               ; 85 fb                       ; 0xc219d
    je short 021a3h                           ; 74 02                       ; 0xc219f
    mov cl, dh                                ; 88 f1                       ; 0xc21a1 vgabios.c:1495
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc21a3 vgabios.c:1497
    add bx, word [bp-006h]                    ; 03 5e fa                    ; 0xc21a6
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc21a9 vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc21ac
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc21ae
    shr dl, 1                                 ; d0 ea                       ; 0xc21b1 vgabios.c:1498
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc21b3 vgabios.c:1499
    jmp short 02188h                          ; eb d1                       ; 0xc21b5
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc21b7 vgabios.c:1500
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc21b9
    jnc short 021d8h                          ; 73 1b                       ; 0xc21bb
    movzx cx, al                              ; 0f b6 c8                    ; 0xc21bd
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08                 ; 0xc21c0
    imul bx, cx                               ; 0f af d9                    ; 0xc21c4
    sal bx, 003h                              ; c1 e3 03                    ; 0xc21c7
    mov cx, word [bp-004h]                    ; 8b 4e fc                    ; 0xc21ca
    add cx, bx                                ; 01 d9                       ; 0xc21cd
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc21cf
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc21d2
    xor ah, ah                                ; 30 e4                       ; 0xc21d4
    jmp short 0218dh                          ; eb b5                       ; 0xc21d6
    leave                                     ; c9                          ; 0xc21d8 vgabios.c:1501
    pop di                                    ; 5f                          ; 0xc21d9
    pop si                                    ; 5e                          ; 0xc21da
    retn 00002h                               ; c2 02 00                    ; 0xc21db
  ; disGetNextSymbol 0xc21de LB 0x1e47 -> off=0x0 cb=0000000000000166 uValue=00000000000c21de 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc21de LB 0x166
    push bp                                   ; 55                          ; 0xc21de vgabios.c:1504
    mov bp, sp                                ; 89 e5                       ; 0xc21df
    push si                                   ; 56                          ; 0xc21e1
    push di                                   ; 57                          ; 0xc21e2
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc21e3
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc21e6
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc21e9
    mov byte [bp-012h], bl                    ; 88 5e ee                    ; 0xc21ec
    mov si, cx                                ; 89 ce                       ; 0xc21ef
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc21f1 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc21f4
    mov es, ax                                ; 8e c0                       ; 0xc21f7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc21f9
    xor ah, ah                                ; 30 e4                       ; 0xc21fc vgabios.c:1512
    call 033c0h                               ; e8 bf 11                    ; 0xc21fe
    mov cl, al                                ; 88 c1                       ; 0xc2201
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2203
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2206 vgabios.c:1513
    je near 0233dh                            ; 0f 84 31 01                 ; 0xc2208
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc220c vgabios.c:1516
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc220f
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc2212
    call 00a08h                               ; e8 f0 e7                    ; 0xc2215
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2218 vgabios.c:1517
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc221b
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc221e
    xor dl, dl                                ; 30 d2                       ; 0xc2221
    shr dx, 008h                              ; c1 ea 08                    ; 0xc2223
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc2226
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2229 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc222c
    mov es, ax                                ; 8e c0                       ; 0xc222f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2231
    xor ah, ah                                ; 30 e4                       ; 0xc2234 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc2236
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2237
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc223a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc223d
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2240 vgabios.c:48
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc2243 vgabios.c:1523
    mov di, bx                                ; 89 df                       ; 0xc2246
    sal di, 003h                              ; c1 e7 03                    ; 0xc2248
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc224b
    jne short 02299h                          ; 75 47                       ; 0xc2250
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc2252 vgabios.c:1526
    imul bx, ax                               ; 0f af d8                    ; 0xc2255
    add bx, bx                                ; 01 db                       ; 0xc2258
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc225a
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc225d
    inc bx                                    ; 43                          ; 0xc2261
    imul bx, cx                               ; 0f af d9                    ; 0xc2262
    xor dh, dh                                ; 30 f6                       ; 0xc2265
    imul ax, dx                               ; 0f af c2                    ; 0xc2267
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc226a
    add ax, dx                                ; 01 d0                       ; 0xc226e
    add ax, ax                                ; 01 c0                       ; 0xc2270
    mov dx, bx                                ; 89 da                       ; 0xc2272
    add dx, ax                                ; 01 c2                       ; 0xc2274
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc2276 vgabios.c:1528
    sal ax, 008h                              ; c1 e0 08                    ; 0xc227a
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc227d
    add ax, bx                                ; 01 d8                       ; 0xc2281
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2283
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2286 vgabios.c:1529
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2289
    mov cx, si                                ; 89 f1                       ; 0xc228d
    mov di, dx                                ; 89 d7                       ; 0xc228f
    cld                                       ; fc                          ; 0xc2291
    jcxz 02296h                               ; e3 02                       ; 0xc2292
    rep stosw                                 ; f3 ab                       ; 0xc2294
    jmp near 0233dh                           ; e9 a4 00                    ; 0xc2296 vgabios.c:1531
    movzx bx, byte [bx+0482eh]                ; 0f b6 9f 2e 48              ; 0xc2299 vgabios.c:1534
    sal bx, 006h                              ; c1 e3 06                    ; 0xc229e
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc22a1
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc22a5
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc22a8 vgabios.c:1535
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc22ac
    dec si                                    ; 4e                          ; 0xc22af vgabios.c:1536
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc22b0
    je near 0233dh                            ; 0f 84 86 00                 ; 0xc22b3
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc22b7 vgabios.c:1538
    sal bx, 003h                              ; c1 e3 03                    ; 0xc22bb
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc22be
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc22c2
    jc short 022d2h                           ; 72 0c                       ; 0xc22c4
    jbe short 022d8h                          ; 76 10                       ; 0xc22c6
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc22c8
    je short 0231fh                           ; 74 53                       ; 0xc22ca
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc22cc
    je short 022dch                           ; 74 0c                       ; 0xc22ce
    jmp short 02337h                          ; eb 65                       ; 0xc22d0
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc22d2
    je short 02300h                           ; 74 2a                       ; 0xc22d4
    jmp short 02337h                          ; eb 5f                       ; 0xc22d6
    or byte [bp-012h], 001h                   ; 80 4e ee 01                 ; 0xc22d8 vgabios.c:1541
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc22dc vgabios.c:1543
    push ax                                   ; 50                          ; 0xc22e0
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc22e1
    push ax                                   ; 50                          ; 0xc22e5
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc22e6
    push ax                                   ; 50                          ; 0xc22ea
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc22eb
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc22ef
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc22f3
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc22f7
    call 01f7dh                               ; e8 7f fc                    ; 0xc22fb
    jmp short 02337h                          ; eb 37                       ; 0xc22fe vgabios.c:1544
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc2300 vgabios.c:1546
    push ax                                   ; 50                          ; 0xc2304
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2305
    push ax                                   ; 50                          ; 0xc2309
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc230a
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc230e
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc2312
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2316
    call 0207ch                               ; e8 5f fd                    ; 0xc231a
    jmp short 02337h                          ; eb 18                       ; 0xc231d vgabios.c:1547
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc231f vgabios.c:1549
    push ax                                   ; 50                          ; 0xc2323
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc2324
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2328
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc232c
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2330
    call 02159h                               ; e8 22 fe                    ; 0xc2334
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc2337 vgabios.c:1556
    jmp near 022afh                           ; e9 72 ff                    ; 0xc233a vgabios.c:1557
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc233d vgabios.c:1559
    pop di                                    ; 5f                          ; 0xc2340
    pop si                                    ; 5e                          ; 0xc2341
    pop bp                                    ; 5d                          ; 0xc2342
    retn                                      ; c3                          ; 0xc2343
  ; disGetNextSymbol 0xc2344 LB 0x1ce1 -> off=0x0 cb=0000000000000162 uValue=00000000000c2344 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2344 LB 0x162
    push bp                                   ; 55                          ; 0xc2344 vgabios.c:1562
    mov bp, sp                                ; 89 e5                       ; 0xc2345
    push si                                   ; 56                          ; 0xc2347
    push di                                   ; 57                          ; 0xc2348
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2349
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc234c
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc234f
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2352
    mov si, cx                                ; 89 ce                       ; 0xc2355
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2357 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc235a
    mov es, ax                                ; 8e c0                       ; 0xc235d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc235f
    xor ah, ah                                ; 30 e4                       ; 0xc2362 vgabios.c:1570
    call 033c0h                               ; e8 59 10                    ; 0xc2364
    mov cl, al                                ; 88 c1                       ; 0xc2367
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2369
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc236c vgabios.c:1571
    je near 0249fh                            ; 0f 84 2d 01                 ; 0xc236e
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2372 vgabios.c:1574
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc2375
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc2378
    call 00a08h                               ; e8 8a e6                    ; 0xc237b
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc237e vgabios.c:1575
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2381
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc2384
    xor dl, dl                                ; 30 d2                       ; 0xc2387
    shr dx, 008h                              ; c1 ea 08                    ; 0xc2389
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc238c
    mov bx, 00084h                            ; bb 84 00                    ; 0xc238f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2392
    mov es, ax                                ; 8e c0                       ; 0xc2395
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2397
    xor ah, ah                                ; 30 e4                       ; 0xc239a vgabios.c:38
    mov di, ax                                ; 89 c7                       ; 0xc239c
    inc di                                    ; 47                          ; 0xc239e
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc239f vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc23a2
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc23a5 vgabios.c:48
    xor ch, ch                                ; 30 ed                       ; 0xc23a8 vgabios.c:1581
    mov bx, cx                                ; 89 cb                       ; 0xc23aa
    sal bx, 003h                              ; c1 e3 03                    ; 0xc23ac
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc23af
    jne short 023f3h                          ; 75 3d                       ; 0xc23b4
    imul di, ax                               ; 0f af f8                    ; 0xc23b6 vgabios.c:1584
    add di, di                                ; 01 ff                       ; 0xc23b9
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc23bb
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc23bf
    inc di                                    ; 47                          ; 0xc23c3
    imul bx, di                               ; 0f af df                    ; 0xc23c4
    xor dh, dh                                ; 30 f6                       ; 0xc23c7
    imul ax, dx                               ; 0f af c2                    ; 0xc23c9
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc23cc
    add ax, dx                                ; 01 d0                       ; 0xc23d0
    add ax, ax                                ; 01 c0                       ; 0xc23d2
    add bx, ax                                ; 01 c3                       ; 0xc23d4
    dec si                                    ; 4e                          ; 0xc23d6 vgabios.c:1586
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc23d7
    je near 0249fh                            ; 0f 84 c1 00                 ; 0xc23da
    movzx di, byte [bp-012h]                  ; 0f b6 7e ee                 ; 0xc23de vgabios.c:1587
    sal di, 003h                              ; c1 e7 03                    ; 0xc23e2
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc23e5 vgabios.c:40
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc23e9
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc23ec
    inc bx                                    ; 43                          ; 0xc23ef vgabios.c:1588
    inc bx                                    ; 43                          ; 0xc23f0
    jmp short 023d6h                          ; eb e3                       ; 0xc23f1 vgabios.c:1589
    mov di, cx                                ; 89 cf                       ; 0xc23f3 vgabios.c:1594
    movzx ax, byte [di+0482eh]                ; 0f b6 85 2e 48              ; 0xc23f5
    mov di, ax                                ; 89 c7                       ; 0xc23fa
    sal di, 006h                              ; c1 e7 06                    ; 0xc23fc
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc23ff
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2403
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2406 vgabios.c:1595
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc240a
    dec si                                    ; 4e                          ; 0xc240d vgabios.c:1596
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc240e
    je near 0249fh                            ; 0f 84 8a 00                 ; 0xc2411
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc2415 vgabios.c:1598
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2419
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc241c
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2420
    jc short 02433h                           ; 72 0e                       ; 0xc2423
    jbe short 0243ah                          ; 76 13                       ; 0xc2425
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2427
    je short 02481h                           ; 74 55                       ; 0xc242a
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc242c
    je short 0243eh                           ; 74 0d                       ; 0xc242f
    jmp short 02499h                          ; eb 66                       ; 0xc2431
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2433
    je short 02462h                           ; 74 2a                       ; 0xc2436
    jmp short 02499h                          ; eb 5f                       ; 0xc2438
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc243a vgabios.c:1601
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc243e vgabios.c:1603
    push ax                                   ; 50                          ; 0xc2442
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc2443
    push ax                                   ; 50                          ; 0xc2447
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2448
    push ax                                   ; 50                          ; 0xc244c
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc244d
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2451
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2455
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2459
    call 01f7dh                               ; e8 1d fb                    ; 0xc245d
    jmp short 02499h                          ; eb 37                       ; 0xc2460 vgabios.c:1604
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc2462 vgabios.c:1606
    push ax                                   ; 50                          ; 0xc2466
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2467
    push ax                                   ; 50                          ; 0xc246b
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc246c
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2470
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2474
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2478
    call 0207ch                               ; e8 fd fb                    ; 0xc247c
    jmp short 02499h                          ; eb 18                       ; 0xc247f vgabios.c:1607
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2481 vgabios.c:1609
    push ax                                   ; 50                          ; 0xc2485
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc2486
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc248a
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc248e
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2492
    call 02159h                               ; e8 c0 fc                    ; 0xc2496
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc2499 vgabios.c:1616
    jmp near 0240dh                           ; e9 6e ff                    ; 0xc249c vgabios.c:1617
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc249f vgabios.c:1619
    pop di                                    ; 5f                          ; 0xc24a2
    pop si                                    ; 5e                          ; 0xc24a3
    pop bp                                    ; 5d                          ; 0xc24a4
    retn                                      ; c3                          ; 0xc24a5
  ; disGetNextSymbol 0xc24a6 LB 0x1b7f -> off=0x0 cb=0000000000000165 uValue=00000000000c24a6 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc24a6 LB 0x165
    push bp                                   ; 55                          ; 0xc24a6 vgabios.c:1622
    mov bp, sp                                ; 89 e5                       ; 0xc24a7
    push si                                   ; 56                          ; 0xc24a9
    push ax                                   ; 50                          ; 0xc24aa
    push ax                                   ; 50                          ; 0xc24ab
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc24ac
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc24af
    mov dx, bx                                ; 89 da                       ; 0xc24b2
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc24b4 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24b7
    mov es, ax                                ; 8e c0                       ; 0xc24ba
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc24bc
    xor ah, ah                                ; 30 e4                       ; 0xc24bf vgabios.c:1629
    call 033c0h                               ; e8 fc 0e                    ; 0xc24c1
    mov ah, al                                ; 88 c4                       ; 0xc24c4
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc24c6 vgabios.c:1630
    je near 025e6h                            ; 0f 84 1a 01                 ; 0xc24c8
    movzx bx, al                              ; 0f b6 d8                    ; 0xc24cc vgabios.c:1631
    sal bx, 003h                              ; c1 e3 03                    ; 0xc24cf
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc24d2
    je near 025e6h                            ; 0f 84 0b 01                 ; 0xc24d7
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc24db vgabios.c:1633
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc24df
    jc short 024f2h                           ; 72 0f                       ; 0xc24e1
    jbe short 024f9h                          ; 76 14                       ; 0xc24e3
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc24e5
    je near 025ech                            ; 0f 84 01 01                 ; 0xc24e7
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc24eb
    je short 024f9h                           ; 74 0a                       ; 0xc24ed
    jmp near 025e6h                           ; e9 f4 00                    ; 0xc24ef
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc24f2
    je short 02568h                           ; 74 72                       ; 0xc24f4
    jmp near 025e6h                           ; e9 ed 00                    ; 0xc24f6
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc24f9 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24fc
    mov es, ax                                ; 8e c0                       ; 0xc24ff
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2501
    imul ax, cx                               ; 0f af c1                    ; 0xc2504 vgabios.c:48
    mov bx, dx                                ; 89 d3                       ; 0xc2507
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2509
    add bx, ax                                ; 01 c3                       ; 0xc250c
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc250e vgabios.c:47
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc2511
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2514 vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc2518
    add bx, ax                                ; 01 c3                       ; 0xc251b
    mov cl, dl                                ; 88 d1                       ; 0xc251d vgabios.c:1639
    and cl, 007h                              ; 80 e1 07                    ; 0xc251f
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2522
    sar ax, CL                                ; d3 f8                       ; 0xc2525
    xor ah, ah                                ; 30 e4                       ; 0xc2527 vgabios.c:1640
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2529
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc252c
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc252e
    out DX, ax                                ; ef                          ; 0xc2531
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2532 vgabios.c:1641
    out DX, ax                                ; ef                          ; 0xc2535
    mov dx, bx                                ; 89 da                       ; 0xc2536 vgabios.c:1642
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2538
    call 033e7h                               ; e8 a9 0e                    ; 0xc253b
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc253e vgabios.c:1643
    je short 0254bh                           ; 74 07                       ; 0xc2542
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2544 vgabios.c:1645
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2547
    out DX, ax                                ; ef                          ; 0xc254a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc254b vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc254e
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2550
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2553
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2556 vgabios.c:1648
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2559
    out DX, ax                                ; ef                          ; 0xc255c
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc255d vgabios.c:1649
    out DX, ax                                ; ef                          ; 0xc2560
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2561 vgabios.c:1650
    out DX, ax                                ; ef                          ; 0xc2564
    jmp near 025e6h                           ; e9 7e 00                    ; 0xc2565 vgabios.c:1651
    mov si, cx                                ; 89 ce                       ; 0xc2568 vgabios.c:1653
    shr si, 1                                 ; d1 ee                       ; 0xc256a
    imul si, si, strict byte 00050h           ; 6b f6 50                    ; 0xc256c
    cmp al, byte [bx+047b1h]                  ; 3a 87 b1 47                 ; 0xc256f
    jne short 0257ch                          ; 75 07                       ; 0xc2573
    mov bx, dx                                ; 89 d3                       ; 0xc2575 vgabios.c:1655
    shr bx, 002h                              ; c1 eb 02                    ; 0xc2577
    jmp short 02581h                          ; eb 05                       ; 0xc257a vgabios.c:1657
    mov bx, dx                                ; 89 d3                       ; 0xc257c vgabios.c:1659
    shr bx, 003h                              ; c1 eb 03                    ; 0xc257e
    add bx, si                                ; 01 f3                       ; 0xc2581
    test cl, 001h                             ; f6 c1 01                    ; 0xc2583 vgabios.c:1661
    je short 0258bh                           ; 74 03                       ; 0xc2586
    add bh, 020h                              ; 80 c7 20                    ; 0xc2588
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc258b vgabios.c:37
    mov es, cx                                ; 8e c1                       ; 0xc258e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2590
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2593 vgabios.c:1663
    sal si, 003h                              ; c1 e6 03                    ; 0xc2596
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc2599
    jne short 025b7h                          ; 75 17                       ; 0xc259e
    mov ah, dl                                ; 88 d4                       ; 0xc25a0 vgabios.c:1665
    and ah, 003h                              ; 80 e4 03                    ; 0xc25a2
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc25a5
    sub cl, ah                                ; 28 e1                       ; 0xc25a7
    add cl, cl                                ; 00 c9                       ; 0xc25a9
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc25ab
    and dh, 003h                              ; 80 e6 03                    ; 0xc25ae
    sal dh, CL                                ; d2 e6                       ; 0xc25b1
    mov DL, strict byte 003h                  ; b2 03                       ; 0xc25b3 vgabios.c:1666
    jmp short 025cah                          ; eb 13                       ; 0xc25b5 vgabios.c:1668
    mov ah, dl                                ; 88 d4                       ; 0xc25b7 vgabios.c:1670
    and ah, 007h                              ; 80 e4 07                    ; 0xc25b9
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc25bc
    sub cl, ah                                ; 28 e1                       ; 0xc25be
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc25c0
    and dh, 001h                              ; 80 e6 01                    ; 0xc25c3
    sal dh, CL                                ; d2 e6                       ; 0xc25c6
    mov DL, strict byte 001h                  ; b2 01                       ; 0xc25c8 vgabios.c:1671
    sal dl, CL                                ; d2 e2                       ; 0xc25ca
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc25cc vgabios.c:1673
    je short 025d6h                           ; 74 04                       ; 0xc25d0
    xor al, dh                                ; 30 f0                       ; 0xc25d2 vgabios.c:1675
    jmp short 025deh                          ; eb 08                       ; 0xc25d4 vgabios.c:1677
    mov ah, dl                                ; 88 d4                       ; 0xc25d6 vgabios.c:1679
    not ah                                    ; f6 d4                       ; 0xc25d8
    and al, ah                                ; 20 e0                       ; 0xc25da
    or al, dh                                 ; 08 f0                       ; 0xc25dc vgabios.c:1680
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc25de vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc25e1
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc25e3
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc25e6 vgabios.c:1683
    pop si                                    ; 5e                          ; 0xc25e9
    pop bp                                    ; 5d                          ; 0xc25ea
    retn                                      ; c3                          ; 0xc25eb
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc25ec vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25ef
    mov es, ax                                ; 8e c0                       ; 0xc25f2
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc25f4
    sal ax, 003h                              ; c1 e0 03                    ; 0xc25f7 vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc25fa
    mov bx, dx                                ; 89 d3                       ; 0xc25fd
    add bx, ax                                ; 01 c3                       ; 0xc25ff
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2601 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc2604
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2606
    jmp short 025e3h                          ; eb d8                       ; 0xc2609
  ; disGetNextSymbol 0xc260b LB 0x1a1a -> off=0x0 cb=000000000000024a uValue=00000000000c260b 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc260b LB 0x24a
    push bp                                   ; 55                          ; 0xc260b vgabios.c:1696
    mov bp, sp                                ; 89 e5                       ; 0xc260c
    push si                                   ; 56                          ; 0xc260e
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc260f
    mov ch, al                                ; 88 c5                       ; 0xc2612
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2614
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc2617
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc261a vgabios.c:1704
    jne short 0262dh                          ; 75 0e                       ; 0xc261d
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc261f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2622
    mov es, ax                                ; 8e c0                       ; 0xc2625
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2627
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc262a vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc262d vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2630
    mov es, ax                                ; 8e c0                       ; 0xc2633
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2635
    xor ah, ah                                ; 30 e4                       ; 0xc2638 vgabios.c:1709
    call 033c0h                               ; e8 83 0d                    ; 0xc263a
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc263d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2640 vgabios.c:1710
    je near 0284fh                            ; 0f 84 09 02                 ; 0xc2642
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2646 vgabios.c:1713
    lea bx, [bp-012h]                         ; 8d 5e ee                    ; 0xc264a
    lea dx, [bp-014h]                         ; 8d 56 ec                    ; 0xc264d
    call 00a08h                               ; e8 b5 e3                    ; 0xc2650
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2653 vgabios.c:1714
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2656
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2659
    xor al, al                                ; 30 c0                       ; 0xc265c
    shr ax, 008h                              ; c1 e8 08                    ; 0xc265e
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2661
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2664 vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2667
    mov es, dx                                ; 8e c2                       ; 0xc266a
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc266c
    xor dh, dh                                ; 30 f6                       ; 0xc266f vgabios.c:38
    inc dx                                    ; 42                          ; 0xc2671
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc2672
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2675 vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2678
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc267b vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc267e vgabios.c:1720
    jc short 02691h                           ; 72 0e                       ; 0xc2681
    jbe short 0269ah                          ; 76 15                       ; 0xc2683
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc2685
    je short 026b0h                           ; 74 26                       ; 0xc2688
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc268a
    je short 026a8h                           ; 74 19                       ; 0xc268d
    jmp short 026b7h                          ; eb 26                       ; 0xc268f
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2691
    je near 027abh                            ; 0f 84 13 01                 ; 0xc2694
    jmp short 026b7h                          ; eb 1d                       ; 0xc2698
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc269a vgabios.c:1727
    jbe near 027abh                           ; 0f 86 09 01                 ; 0xc269e
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc26a2
    jmp near 027abh                           ; e9 03 01                    ; 0xc26a5 vgabios.c:1728
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc26a8 vgabios.c:1731
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc26aa
    jmp near 027abh                           ; e9 fb 00                    ; 0xc26ad vgabios.c:1732
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc26b0 vgabios.c:1735
    jmp near 027abh                           ; e9 f4 00                    ; 0xc26b4 vgabios.c:1736
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc26b7 vgabios.c:1740
    mov bx, si                                ; 89 f3                       ; 0xc26bb
    sal bx, 003h                              ; c1 e3 03                    ; 0xc26bd
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc26c0
    jne short 0270ah                          ; 75 43                       ; 0xc26c5
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc26c7 vgabios.c:1743
    imul ax, word [bp-00eh]                   ; 0f af 46 f2                 ; 0xc26ca
    add ax, ax                                ; 01 c0                       ; 0xc26ce
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc26d0
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc26d2
    mov si, ax                                ; 89 c6                       ; 0xc26d6
    inc si                                    ; 46                          ; 0xc26d8
    imul si, dx                               ; 0f af f2                    ; 0xc26d9
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc26dc
    imul ax, word [bp-010h]                   ; 0f af 46 f0                 ; 0xc26e0
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc26e4
    add ax, dx                                ; 01 d0                       ; 0xc26e8
    add ax, ax                                ; 01 c0                       ; 0xc26ea
    add si, ax                                ; 01 c6                       ; 0xc26ec
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc26ee vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc26f2
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc26f5 vgabios.c:1748
    jne near 02798h                           ; 0f 85 9c 00                 ; 0xc26f8
    inc si                                    ; 46                          ; 0xc26fc vgabios.c:1749
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc26fd vgabios.c:40
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2701
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2704
    jmp near 02798h                           ; e9 8e 00                    ; 0xc2707 vgabios.c:1751
    movzx si, byte [si+0482eh]                ; 0f b6 b4 2e 48              ; 0xc270a vgabios.c:1754
    sal si, 006h                              ; c1 e6 06                    ; 0xc270f
    mov ah, byte [si+04844h]                  ; 8a a4 44 48                 ; 0xc2712
    mov dl, byte [bx+047b1h]                  ; 8a 97 b1 47                 ; 0xc2716 vgabios.c:1755
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc271a vgabios.c:1756
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc271e
    jc short 0272eh                           ; 72 0c                       ; 0xc2720
    jbe short 02734h                          ; 76 10                       ; 0xc2722
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2724
    je short 0277fh                           ; 74 57                       ; 0xc2726
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2728
    je short 02738h                           ; 74 0c                       ; 0xc272a
    jmp short 02798h                          ; eb 6a                       ; 0xc272c
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc272e
    je short 0275eh                           ; 74 2c                       ; 0xc2730
    jmp short 02798h                          ; eb 64                       ; 0xc2732
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2734 vgabios.c:1759
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2738 vgabios.c:1761
    push dx                                   ; 52                          ; 0xc273c
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc273d
    push ax                                   ; 50                          ; 0xc2740
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2741
    push ax                                   ; 50                          ; 0xc2745
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc                 ; 0xc2746
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa                 ; 0xc274a
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc274e
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc2752
    mov cx, bx                                ; 89 d9                       ; 0xc2755
    mov bx, si                                ; 89 f3                       ; 0xc2757
    call 01f7dh                               ; e8 21 f8                    ; 0xc2759
    jmp short 02798h                          ; eb 3a                       ; 0xc275c vgabios.c:1762
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc275e vgabios.c:1764
    push ax                                   ; 50                          ; 0xc2761
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2762
    push ax                                   ; 50                          ; 0xc2766
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2767
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc276b
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc276f
    movzx si, ch                              ; 0f b6 f5                    ; 0xc2773
    mov cx, ax                                ; 89 c1                       ; 0xc2776
    mov ax, si                                ; 89 f0                       ; 0xc2778
    call 0207ch                               ; e8 ff f8                    ; 0xc277a
    jmp short 02798h                          ; eb 19                       ; 0xc277d vgabios.c:1765
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc277f vgabios.c:1767
    push ax                                   ; 50                          ; 0xc2783
    movzx si, byte [bp-004h]                  ; 0f b6 76 fc                 ; 0xc2784
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2788
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc278c
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc2790
    mov cx, si                                ; 89 f1                       ; 0xc2793
    call 02159h                               ; e8 c1 f9                    ; 0xc2795
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2798 vgabios.c:1775
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc279b vgabios.c:1777
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc279f
    jne short 027abh                          ; 75 07                       ; 0xc27a2
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc27a4 vgabios.c:1778
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc27a8 vgabios.c:1779
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc27ab vgabios.c:1784
    cmp ax, word [bp-00eh]                    ; 3b 46 f2                    ; 0xc27af
    jne near 02833h                           ; 0f 85 7d 00                 ; 0xc27b2
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc27b6 vgabios.c:1786
    sal bx, 003h                              ; c1 e3 03                    ; 0xc27ba
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc27bd
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc27c0
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc27c2
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc27c5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc27c7
    jne short 02816h                          ; 75 48                       ; 0xc27cc
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc27ce vgabios.c:1788
    imul dx, word [bp-00eh]                   ; 0f af 56 f2                 ; 0xc27d1
    add dx, dx                                ; 01 d2                       ; 0xc27d5
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc27d7
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc27da
    inc dx                                    ; 42                          ; 0xc27de
    imul si, dx                               ; 0f af f2                    ; 0xc27df
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc27e2
    dec dx                                    ; 4a                          ; 0xc27e6
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc27e7
    imul cx, dx                               ; 0f af ca                    ; 0xc27ea
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc27ed
    add dx, cx                                ; 01 ca                       ; 0xc27f1
    add dx, dx                                ; 01 d2                       ; 0xc27f3
    add si, dx                                ; 01 d6                       ; 0xc27f5
    inc si                                    ; 46                          ; 0xc27f7 vgabios.c:1789
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc27f8 vgabios.c:35
    mov bl, byte [es:si]                      ; 26 8a 1c                    ; 0xc27fc
    push strict byte 00001h                   ; 6a 01                       ; 0xc27ff vgabios.c:1790
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2801
    push dx                                   ; 52                          ; 0xc2805
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc2806
    push dx                                   ; 52                          ; 0xc2809
    xor ah, ah                                ; 30 e4                       ; 0xc280a
    push ax                                   ; 50                          ; 0xc280c
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc280d
    xor cx, cx                                ; 31 c9                       ; 0xc2810
    xor bx, bx                                ; 31 db                       ; 0xc2812
    jmp short 0282ah                          ; eb 14                       ; 0xc2814 vgabios.c:1792
    push strict byte 00001h                   ; 6a 01                       ; 0xc2816 vgabios.c:1794
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2818
    push dx                                   ; 52                          ; 0xc281c
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc281d
    push dx                                   ; 52                          ; 0xc2820
    xor ah, ah                                ; 30 e4                       ; 0xc2821
    push ax                                   ; 50                          ; 0xc2823
    xor cx, cx                                ; 31 c9                       ; 0xc2824
    xor bx, bx                                ; 31 db                       ; 0xc2826
    xor dx, dx                                ; 31 d2                       ; 0xc2828
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc282a
    call 0194dh                               ; e8 1d f1                    ; 0xc282d
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc2830 vgabios.c:1796
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2833 vgabios.c:1800
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc2837
    sal word [bp-012h], 008h                  ; c1 66 ee 08                 ; 0xc283a
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc283e
    add word [bp-012h], ax                    ; 01 46 ee                    ; 0xc2842
    mov dx, word [bp-012h]                    ; 8b 56 ee                    ; 0xc2845 vgabios.c:1801
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2848
    call 011b8h                               ; e8 69 e9                    ; 0xc284c
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc284f vgabios.c:1802
    pop si                                    ; 5e                          ; 0xc2852
    pop bp                                    ; 5d                          ; 0xc2853
    retn                                      ; c3                          ; 0xc2854
  ; disGetNextSymbol 0xc2855 LB 0x17d0 -> off=0x0 cb=000000000000002c uValue=00000000000c2855 'get_font_access'
get_font_access:                             ; 0xc2855 LB 0x2c
    push bp                                   ; 55                          ; 0xc2855 vgabios.c:1805
    mov bp, sp                                ; 89 e5                       ; 0xc2856
    push dx                                   ; 52                          ; 0xc2858
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2859 vgabios.c:1807
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc285c
    out DX, ax                                ; ef                          ; 0xc285f
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2860 vgabios.c:1808
    out DX, ax                                ; ef                          ; 0xc2863
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2864 vgabios.c:1809
    out DX, ax                                ; ef                          ; 0xc2867
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2868 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc286b
    mov ax, 00204h                            ; b8 04 02                    ; 0xc286c vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc286f
    out DX, ax                                ; ef                          ; 0xc2872
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2873 vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2876
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2877 vgabios.c:1813
    out DX, ax                                ; ef                          ; 0xc287a
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc287b vgabios.c:1814
    pop dx                                    ; 5a                          ; 0xc287e
    pop bp                                    ; 5d                          ; 0xc287f
    retn                                      ; c3                          ; 0xc2880
  ; disGetNextSymbol 0xc2881 LB 0x17a4 -> off=0x0 cb=000000000000003c uValue=00000000000c2881 'release_font_access'
release_font_access:                         ; 0xc2881 LB 0x3c
    push bp                                   ; 55                          ; 0xc2881 vgabios.c:1816
    mov bp, sp                                ; 89 e5                       ; 0xc2882
    push dx                                   ; 52                          ; 0xc2884
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2885 vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2888
    out DX, ax                                ; ef                          ; 0xc288b
    mov ax, 00302h                            ; b8 02 03                    ; 0xc288c vgabios.c:1819
    out DX, ax                                ; ef                          ; 0xc288f
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2890 vgabios.c:1820
    out DX, ax                                ; ef                          ; 0xc2893
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2894 vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2897
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2898 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc289b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc289c
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc289e
    sal ax, 002h                              ; c1 e0 02                    ; 0xc28a1
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc28a4
    sal ax, 008h                              ; c1 e0 08                    ; 0xc28a6
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc28a9
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc28ab
    out DX, ax                                ; ef                          ; 0xc28ae
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc28af vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc28b2
    mov ax, 01005h                            ; b8 05 10                    ; 0xc28b3 vgabios.c:1824
    out DX, ax                                ; ef                          ; 0xc28b6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc28b7 vgabios.c:1825
    pop dx                                    ; 5a                          ; 0xc28ba
    pop bp                                    ; 5d                          ; 0xc28bb
    retn                                      ; c3                          ; 0xc28bc
  ; disGetNextSymbol 0xc28bd LB 0x1768 -> off=0x0 cb=00000000000000b4 uValue=00000000000c28bd 'set_scan_lines'
set_scan_lines:                              ; 0xc28bd LB 0xb4
    push bp                                   ; 55                          ; 0xc28bd vgabios.c:1827
    mov bp, sp                                ; 89 e5                       ; 0xc28be
    push bx                                   ; 53                          ; 0xc28c0
    push cx                                   ; 51                          ; 0xc28c1
    push dx                                   ; 52                          ; 0xc28c2
    push si                                   ; 56                          ; 0xc28c3
    push di                                   ; 57                          ; 0xc28c4
    mov bl, al                                ; 88 c3                       ; 0xc28c5
    mov si, strict word 00063h                ; be 63 00                    ; 0xc28c7 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28ca
    mov es, ax                                ; 8e c0                       ; 0xc28cd
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc28cf
    mov cx, si                                ; 89 f1                       ; 0xc28d2 vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc28d4 vgabios.c:1833
    mov dx, si                                ; 89 f2                       ; 0xc28d6
    out DX, AL                                ; ee                          ; 0xc28d8
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc28d9 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc28dc
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28dd
    mov ah, al                                ; 88 c4                       ; 0xc28df vgabios.c:1835
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc28e1
    mov al, bl                                ; 88 d8                       ; 0xc28e4
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc28e6
    or al, ah                                 ; 08 e0                       ; 0xc28e8
    out DX, AL                                ; ee                          ; 0xc28ea vgabios.c:1836
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc28eb vgabios.c:1837
    jne short 028f8h                          ; 75 08                       ; 0xc28ee
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc28f0 vgabios.c:1839
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc28f3
    jmp short 02905h                          ; eb 0d                       ; 0xc28f6 vgabios.c:1841
    mov al, bl                                ; 88 d8                       ; 0xc28f8 vgabios.c:1843
    sub AL, strict byte 003h                  ; 2c 03                       ; 0xc28fa
    movzx dx, al                              ; 0f b6 d0                    ; 0xc28fc
    mov al, bl                                ; 88 d8                       ; 0xc28ff
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2901
    xor ah, ah                                ; 30 e4                       ; 0xc2903
    call 010c2h                               ; e8 ba e7                    ; 0xc2905
    movzx di, bl                              ; 0f b6 fb                    ; 0xc2908 vgabios.c:1845
    mov bx, 00085h                            ; bb 85 00                    ; 0xc290b vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc290e
    mov es, ax                                ; 8e c0                       ; 0xc2911
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc2913
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2916 vgabios.c:1846
    mov dx, cx                                ; 89 ca                       ; 0xc2918
    out DX, AL                                ; ee                          ; 0xc291a
    mov bx, cx                                ; 89 cb                       ; 0xc291b vgabios.c:1847
    inc bx                                    ; 43                          ; 0xc291d
    mov dx, bx                                ; 89 da                       ; 0xc291e
    in AL, DX                                 ; ec                          ; 0xc2920
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2921
    mov si, ax                                ; 89 c6                       ; 0xc2923
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2925 vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2927
    out DX, AL                                ; ee                          ; 0xc2929
    mov dx, bx                                ; 89 da                       ; 0xc292a vgabios.c:1849
    in AL, DX                                 ; ec                          ; 0xc292c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc292d
    mov ah, al                                ; 88 c4                       ; 0xc292f vgabios.c:1850
    and ah, 002h                              ; 80 e4 02                    ; 0xc2931
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc2934
    sal dx, 007h                              ; c1 e2 07                    ; 0xc2937
    and AL, strict byte 040h                  ; 24 40                       ; 0xc293a
    xor ah, ah                                ; 30 e4                       ; 0xc293c
    sal ax, 003h                              ; c1 e0 03                    ; 0xc293e
    add ax, dx                                ; 01 d0                       ; 0xc2941
    inc ax                                    ; 40                          ; 0xc2943
    add ax, si                                ; 01 f0                       ; 0xc2944
    xor dx, dx                                ; 31 d2                       ; 0xc2946 vgabios.c:1851
    div di                                    ; f7 f7                       ; 0xc2948
    mov dl, al                                ; 88 c2                       ; 0xc294a vgabios.c:1852
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc294c
    mov bx, 00084h                            ; bb 84 00                    ; 0xc294e vgabios.c:42
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc2951
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2954 vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2957
    xor ah, ah                                ; 30 e4                       ; 0xc295a vgabios.c:1854
    imul dx, ax                               ; 0f af d0                    ; 0xc295c
    add dx, dx                                ; 01 d2                       ; 0xc295f
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2961 vgabios.c:52
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc2964
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2967 vgabios.c:1855
    pop di                                    ; 5f                          ; 0xc296a
    pop si                                    ; 5e                          ; 0xc296b
    pop dx                                    ; 5a                          ; 0xc296c
    pop cx                                    ; 59                          ; 0xc296d
    pop bx                                    ; 5b                          ; 0xc296e
    pop bp                                    ; 5d                          ; 0xc296f
    retn                                      ; c3                          ; 0xc2970
  ; disGetNextSymbol 0xc2971 LB 0x16b4 -> off=0x0 cb=000000000000007d uValue=00000000000c2971 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2971 LB 0x7d
    push bp                                   ; 55                          ; 0xc2971 vgabios.c:1857
    mov bp, sp                                ; 89 e5                       ; 0xc2972
    push si                                   ; 56                          ; 0xc2974
    push di                                   ; 57                          ; 0xc2975
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2976
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2979
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc297c
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc297f
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc2982
    call 02855h                               ; e8 cd fe                    ; 0xc2985 vgabios.c:1862
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2988 vgabios.c:1863
    and AL, strict byte 003h                  ; 24 03                       ; 0xc298b
    xor ah, ah                                ; 30 e4                       ; 0xc298d
    mov bx, ax                                ; 89 c3                       ; 0xc298f
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2991
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2994
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2997
    xor ah, ah                                ; 30 e4                       ; 0xc2999
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc299b
    add bx, ax                                ; 01 c3                       ; 0xc299e
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc29a0
    xor bx, bx                                ; 31 db                       ; 0xc29a3 vgabios.c:1864
    cmp bx, word [bp-00ah]                    ; 3b 5e f6                    ; 0xc29a5
    jnc short 029d5h                          ; 73 2b                       ; 0xc29a8
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc29aa vgabios.c:1866
    mov si, bx                                ; 89 de                       ; 0xc29ae
    imul si, cx                               ; 0f af f1                    ; 0xc29b0
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc29b3
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc29b6 vgabios.c:1867
    add di, bx                                ; 01 df                       ; 0xc29b9
    sal di, 005h                              ; c1 e7 05                    ; 0xc29bb
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc29be
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc29c1 vgabios.c:1868
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc29c4
    mov es, ax                                ; 8e c0                       ; 0xc29c7
    cld                                       ; fc                          ; 0xc29c9
    jcxz 029d2h                               ; e3 06                       ; 0xc29ca
    push DS                                   ; 1e                          ; 0xc29cc
    mov ds, dx                                ; 8e da                       ; 0xc29cd
    rep movsb                                 ; f3 a4                       ; 0xc29cf
    pop DS                                    ; 1f                          ; 0xc29d1
    inc bx                                    ; 43                          ; 0xc29d2 vgabios.c:1869
    jmp short 029a5h                          ; eb d0                       ; 0xc29d3
    call 02881h                               ; e8 a9 fe                    ; 0xc29d5 vgabios.c:1870
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc29d8 vgabios.c:1871
    jc short 029e5h                           ; 72 07                       ; 0xc29dc
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08                 ; 0xc29de vgabios.c:1873
    call 028bdh                               ; e8 d8 fe                    ; 0xc29e2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc29e5 vgabios.c:1875
    pop di                                    ; 5f                          ; 0xc29e8
    pop si                                    ; 5e                          ; 0xc29e9
    pop bp                                    ; 5d                          ; 0xc29ea
    retn 00006h                               ; c2 06 00                    ; 0xc29eb
  ; disGetNextSymbol 0xc29ee LB 0x1637 -> off=0x0 cb=0000000000000070 uValue=00000000000c29ee 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc29ee LB 0x70
    push bp                                   ; 55                          ; 0xc29ee vgabios.c:1877
    mov bp, sp                                ; 89 e5                       ; 0xc29ef
    push bx                                   ; 53                          ; 0xc29f1
    push cx                                   ; 51                          ; 0xc29f2
    push si                                   ; 56                          ; 0xc29f3
    push di                                   ; 57                          ; 0xc29f4
    push ax                                   ; 50                          ; 0xc29f5
    push ax                                   ; 50                          ; 0xc29f6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc29f7
    call 02855h                               ; e8 58 fe                    ; 0xc29fa vgabios.c:1881
    mov al, dl                                ; 88 d0                       ; 0xc29fd vgabios.c:1882
    and AL, strict byte 003h                  ; 24 03                       ; 0xc29ff
    xor ah, ah                                ; 30 e4                       ; 0xc2a01
    mov bx, ax                                ; 89 c3                       ; 0xc2a03
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2a05
    mov al, dl                                ; 88 d0                       ; 0xc2a08
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2a0a
    xor ah, ah                                ; 30 e4                       ; 0xc2a0c
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2a0e
    add bx, ax                                ; 01 c3                       ; 0xc2a11
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2a13
    xor bx, bx                                ; 31 db                       ; 0xc2a16 vgabios.c:1883
    jmp short 02a20h                          ; eb 06                       ; 0xc2a18
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2a1a
    jnc short 02a46h                          ; 73 26                       ; 0xc2a1e
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2a20 vgabios.c:1885
    mov di, bx                                ; 89 df                       ; 0xc2a23 vgabios.c:1886
    sal di, 005h                              ; c1 e7 05                    ; 0xc2a25
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2a28
    add si, 05d6ch                            ; 81 c6 6c 5d                 ; 0xc2a2b vgabios.c:1887
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2a2f
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2a32
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2a35
    mov es, ax                                ; 8e c0                       ; 0xc2a38
    cld                                       ; fc                          ; 0xc2a3a
    jcxz 02a43h                               ; e3 06                       ; 0xc2a3b
    push DS                                   ; 1e                          ; 0xc2a3d
    mov ds, dx                                ; 8e da                       ; 0xc2a3e
    rep movsb                                 ; f3 a4                       ; 0xc2a40
    pop DS                                    ; 1f                          ; 0xc2a42
    inc bx                                    ; 43                          ; 0xc2a43 vgabios.c:1888
    jmp short 02a1ah                          ; eb d4                       ; 0xc2a44
    call 02881h                               ; e8 38 fe                    ; 0xc2a46 vgabios.c:1889
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2a49 vgabios.c:1890
    jc short 02a55h                           ; 72 06                       ; 0xc2a4d
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2a4f vgabios.c:1892
    call 028bdh                               ; e8 68 fe                    ; 0xc2a52
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2a55 vgabios.c:1894
    pop di                                    ; 5f                          ; 0xc2a58
    pop si                                    ; 5e                          ; 0xc2a59
    pop cx                                    ; 59                          ; 0xc2a5a
    pop bx                                    ; 5b                          ; 0xc2a5b
    pop bp                                    ; 5d                          ; 0xc2a5c
    retn                                      ; c3                          ; 0xc2a5d
  ; disGetNextSymbol 0xc2a5e LB 0x15c7 -> off=0x0 cb=0000000000000072 uValue=00000000000c2a5e 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2a5e LB 0x72
    push bp                                   ; 55                          ; 0xc2a5e vgabios.c:1896
    mov bp, sp                                ; 89 e5                       ; 0xc2a5f
    push bx                                   ; 53                          ; 0xc2a61
    push cx                                   ; 51                          ; 0xc2a62
    push si                                   ; 56                          ; 0xc2a63
    push di                                   ; 57                          ; 0xc2a64
    push ax                                   ; 50                          ; 0xc2a65
    push ax                                   ; 50                          ; 0xc2a66
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2a67
    call 02855h                               ; e8 e8 fd                    ; 0xc2a6a vgabios.c:1900
    mov al, dl                                ; 88 d0                       ; 0xc2a6d vgabios.c:1901
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2a6f
    xor ah, ah                                ; 30 e4                       ; 0xc2a71
    mov bx, ax                                ; 89 c3                       ; 0xc2a73
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2a75
    mov al, dl                                ; 88 d0                       ; 0xc2a78
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2a7a
    xor ah, ah                                ; 30 e4                       ; 0xc2a7c
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2a7e
    add bx, ax                                ; 01 c3                       ; 0xc2a81
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2a83
    xor bx, bx                                ; 31 db                       ; 0xc2a86 vgabios.c:1902
    jmp short 02a90h                          ; eb 06                       ; 0xc2a88
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2a8a
    jnc short 02ab8h                          ; 73 28                       ; 0xc2a8e
    mov si, bx                                ; 89 de                       ; 0xc2a90 vgabios.c:1904
    sal si, 003h                              ; c1 e6 03                    ; 0xc2a92
    mov di, bx                                ; 89 df                       ; 0xc2a95 vgabios.c:1905
    sal di, 005h                              ; c1 e7 05                    ; 0xc2a97
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2a9a
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2a9d vgabios.c:1906
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2aa1
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2aa4
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2aa7
    mov es, ax                                ; 8e c0                       ; 0xc2aaa
    cld                                       ; fc                          ; 0xc2aac
    jcxz 02ab5h                               ; e3 06                       ; 0xc2aad
    push DS                                   ; 1e                          ; 0xc2aaf
    mov ds, dx                                ; 8e da                       ; 0xc2ab0
    rep movsb                                 ; f3 a4                       ; 0xc2ab2
    pop DS                                    ; 1f                          ; 0xc2ab4
    inc bx                                    ; 43                          ; 0xc2ab5 vgabios.c:1907
    jmp short 02a8ah                          ; eb d2                       ; 0xc2ab6
    call 02881h                               ; e8 c6 fd                    ; 0xc2ab8 vgabios.c:1908
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2abb vgabios.c:1909
    jc short 02ac7h                           ; 72 06                       ; 0xc2abf
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2ac1 vgabios.c:1911
    call 028bdh                               ; e8 f6 fd                    ; 0xc2ac4
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2ac7 vgabios.c:1913
    pop di                                    ; 5f                          ; 0xc2aca
    pop si                                    ; 5e                          ; 0xc2acb
    pop cx                                    ; 59                          ; 0xc2acc
    pop bx                                    ; 5b                          ; 0xc2acd
    pop bp                                    ; 5d                          ; 0xc2ace
    retn                                      ; c3                          ; 0xc2acf
  ; disGetNextSymbol 0xc2ad0 LB 0x1555 -> off=0x0 cb=0000000000000072 uValue=00000000000c2ad0 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2ad0 LB 0x72
    push bp                                   ; 55                          ; 0xc2ad0 vgabios.c:1916
    mov bp, sp                                ; 89 e5                       ; 0xc2ad1
    push bx                                   ; 53                          ; 0xc2ad3
    push cx                                   ; 51                          ; 0xc2ad4
    push si                                   ; 56                          ; 0xc2ad5
    push di                                   ; 57                          ; 0xc2ad6
    push ax                                   ; 50                          ; 0xc2ad7
    push ax                                   ; 50                          ; 0xc2ad8
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2ad9
    call 02855h                               ; e8 76 fd                    ; 0xc2adc vgabios.c:1920
    mov al, dl                                ; 88 d0                       ; 0xc2adf vgabios.c:1921
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2ae1
    xor ah, ah                                ; 30 e4                       ; 0xc2ae3
    mov bx, ax                                ; 89 c3                       ; 0xc2ae5
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2ae7
    mov al, dl                                ; 88 d0                       ; 0xc2aea
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2aec
    xor ah, ah                                ; 30 e4                       ; 0xc2aee
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2af0
    add bx, ax                                ; 01 c3                       ; 0xc2af3
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2af5
    xor bx, bx                                ; 31 db                       ; 0xc2af8 vgabios.c:1922
    jmp short 02b02h                          ; eb 06                       ; 0xc2afa
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2afc
    jnc short 02b2ah                          ; 73 28                       ; 0xc2b00
    mov si, bx                                ; 89 de                       ; 0xc2b02 vgabios.c:1924
    sal si, 004h                              ; c1 e6 04                    ; 0xc2b04
    mov di, bx                                ; 89 df                       ; 0xc2b07 vgabios.c:1925
    sal di, 005h                              ; c1 e7 05                    ; 0xc2b09
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2b0c
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2b0f vgabios.c:1926
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2b13
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2b16
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2b19
    mov es, ax                                ; 8e c0                       ; 0xc2b1c
    cld                                       ; fc                          ; 0xc2b1e
    jcxz 02b27h                               ; e3 06                       ; 0xc2b1f
    push DS                                   ; 1e                          ; 0xc2b21
    mov ds, dx                                ; 8e da                       ; 0xc2b22
    rep movsb                                 ; f3 a4                       ; 0xc2b24
    pop DS                                    ; 1f                          ; 0xc2b26
    inc bx                                    ; 43                          ; 0xc2b27 vgabios.c:1927
    jmp short 02afch                          ; eb d2                       ; 0xc2b28
    call 02881h                               ; e8 54 fd                    ; 0xc2b2a vgabios.c:1928
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2b2d vgabios.c:1929
    jc short 02b39h                           ; 72 06                       ; 0xc2b31
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2b33 vgabios.c:1931
    call 028bdh                               ; e8 84 fd                    ; 0xc2b36
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2b39 vgabios.c:1933
    pop di                                    ; 5f                          ; 0xc2b3c
    pop si                                    ; 5e                          ; 0xc2b3d
    pop cx                                    ; 59                          ; 0xc2b3e
    pop bx                                    ; 5b                          ; 0xc2b3f
    pop bp                                    ; 5d                          ; 0xc2b40
    retn                                      ; c3                          ; 0xc2b41
  ; disGetNextSymbol 0xc2b42 LB 0x14e3 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b42 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2b42 LB 0x5
    push bp                                   ; 55                          ; 0xc2b42 vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2b43
    pop bp                                    ; 5d                          ; 0xc2b45 vgabios.c:1940
    retn                                      ; c3                          ; 0xc2b46
  ; disGetNextSymbol 0xc2b47 LB 0x14de -> off=0x0 cb=0000000000000007 uValue=00000000000c2b47 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2b47 LB 0x7
    push bp                                   ; 55                          ; 0xc2b47 vgabios.c:1941
    mov bp, sp                                ; 89 e5                       ; 0xc2b48
    pop bp                                    ; 5d                          ; 0xc2b4a vgabios.c:1947
    retn 00002h                               ; c2 02 00                    ; 0xc2b4b
  ; disGetNextSymbol 0xc2b4e LB 0x14d7 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b4e 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2b4e LB 0x5
    push bp                                   ; 55                          ; 0xc2b4e vgabios.c:1948
    mov bp, sp                                ; 89 e5                       ; 0xc2b4f
    pop bp                                    ; 5d                          ; 0xc2b51 vgabios.c:1953
    retn                                      ; c3                          ; 0xc2b52
  ; disGetNextSymbol 0xc2b53 LB 0x14d2 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b53 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2b53 LB 0x5
    push bp                                   ; 55                          ; 0xc2b53 vgabios.c:1954
    mov bp, sp                                ; 89 e5                       ; 0xc2b54
    pop bp                                    ; 5d                          ; 0xc2b56 vgabios.c:1959
    retn                                      ; c3                          ; 0xc2b57
  ; disGetNextSymbol 0xc2b58 LB 0x14cd -> off=0x0 cb=0000000000000005 uValue=00000000000c2b58 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2b58 LB 0x5
    push bp                                   ; 55                          ; 0xc2b58 vgabios.c:1960
    mov bp, sp                                ; 89 e5                       ; 0xc2b59
    pop bp                                    ; 5d                          ; 0xc2b5b vgabios.c:1965
    retn                                      ; c3                          ; 0xc2b5c
  ; disGetNextSymbol 0xc2b5d LB 0x14c8 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b5d 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2b5d LB 0x5
    push bp                                   ; 55                          ; 0xc2b5d vgabios.c:1967
    mov bp, sp                                ; 89 e5                       ; 0xc2b5e
    pop bp                                    ; 5d                          ; 0xc2b60 vgabios.c:1972
    retn                                      ; c3                          ; 0xc2b61
  ; disGetNextSymbol 0xc2b62 LB 0x14c3 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b62 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2b62 LB 0x5
    push bp                                   ; 55                          ; 0xc2b62 vgabios.c:1975
    mov bp, sp                                ; 89 e5                       ; 0xc2b63
    pop bp                                    ; 5d                          ; 0xc2b65 vgabios.c:1980
    retn                                      ; c3                          ; 0xc2b66
  ; disGetNextSymbol 0xc2b67 LB 0x14be -> off=0x0 cb=0000000000000005 uValue=00000000000c2b67 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2b67 LB 0x5
    push bp                                   ; 55                          ; 0xc2b67 vgabios.c:1981
    mov bp, sp                                ; 89 e5                       ; 0xc2b68
    pop bp                                    ; 5d                          ; 0xc2b6a vgabios.c:1986
    retn                                      ; c3                          ; 0xc2b6b
  ; disGetNextSymbol 0xc2b6c LB 0x14b9 -> off=0x0 cb=0000000000000096 uValue=00000000000c2b6c 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2b6c LB 0x96
    push bp                                   ; 55                          ; 0xc2b6c vgabios.c:1989
    mov bp, sp                                ; 89 e5                       ; 0xc2b6d
    push si                                   ; 56                          ; 0xc2b6f
    push di                                   ; 57                          ; 0xc2b70
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2b71
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2b74
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2b77
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2b7a
    mov si, cx                                ; 89 ce                       ; 0xc2b7d
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2b7f
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2b82 vgabios.c:1996
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2b85
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2b88
    call 00a08h                               ; e8 7a de                    ; 0xc2b8b
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2b8e vgabios.c:1999
    jne short 02ba5h                          ; 75 11                       ; 0xc2b92
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2b94 vgabios.c:2000
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2b97
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2b9a vgabios.c:2001
    xor al, al                                ; 30 c0                       ; 0xc2b9d
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2b9f
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2ba2
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc2ba5 vgabios.c:2004
    sal dx, 008h                              ; c1 e2 08                    ; 0xc2ba9
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc2bac
    add dx, ax                                ; 01 c2                       ; 0xc2bb0
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2bb2 vgabios.c:2005
    call 011b8h                               ; e8 ff e5                    ; 0xc2bb6
    dec si                                    ; 4e                          ; 0xc2bb9 vgabios.c:2007
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2bba
    je short 02be9h                           ; 74 2a                       ; 0xc2bbd
    mov bx, di                                ; 89 fb                       ; 0xc2bbf vgabios.c:2009
    inc di                                    ; 47                          ; 0xc2bc1
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2bc2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2bc5
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2bc8 vgabios.c:2010
    je short 02bd7h                           ; 74 09                       ; 0xc2bcc
    mov bx, di                                ; 89 fb                       ; 0xc2bce vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2bd0
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2bd1 vgabios.c:37
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc2bd4 vgabios.c:38
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc2bd7 vgabios.c:2013
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2bdb
    xor ah, ah                                ; 30 e4                       ; 0xc2bdf
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2be1
    call 0260bh                               ; e8 24 fa                    ; 0xc2be4
    jmp short 02bb9h                          ; eb d0                       ; 0xc2be7 vgabios.c:2014
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc2be9 vgabios.c:2017
    jne short 02bf9h                          ; 75 0a                       ; 0xc2bed
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2bef vgabios.c:2018
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2bf2
    call 011b8h                               ; e8 bf e5                    ; 0xc2bf6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2bf9 vgabios.c:2019
    pop di                                    ; 5f                          ; 0xc2bfc
    pop si                                    ; 5e                          ; 0xc2bfd
    pop bp                                    ; 5d                          ; 0xc2bfe
    retn 00008h                               ; c2 08 00                    ; 0xc2bff
  ; disGetNextSymbol 0xc2c02 LB 0x1423 -> off=0x0 cb=00000000000001f5 uValue=00000000000c2c02 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2c02 LB 0x1f5
    push bp                                   ; 55                          ; 0xc2c02 vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2c03
    push cx                                   ; 51                          ; 0xc2c05
    push si                                   ; 56                          ; 0xc2c06
    push di                                   ; 57                          ; 0xc2c07
    push ax                                   ; 50                          ; 0xc2c08
    push ax                                   ; 50                          ; 0xc2c09
    push dx                                   ; 52                          ; 0xc2c0a
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2c0b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c0e
    mov es, ax                                ; 8e c0                       ; 0xc2c11
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2c13
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2c16 vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2c19 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2c1c
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2c1f vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2c22 vgabios.c:2033
    mov es, dx                                ; 8e c2                       ; 0xc2c24 vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2c26
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2c2b
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2c2f vgabios.c:2038
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2c32
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2c35
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2c38
    cld                                       ; fc                          ; 0xc2c3b
    jcxz 02c44h                               ; e3 06                       ; 0xc2c3c
    push DS                                   ; 1e                          ; 0xc2c3e
    mov ds, dx                                ; 8e da                       ; 0xc2c3f
    rep movsb                                 ; f3 a4                       ; 0xc2c41
    pop DS                                    ; 1f                          ; 0xc2c43
    mov si, 00084h                            ; be 84 00                    ; 0xc2c44 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c47
    mov es, ax                                ; 8e c0                       ; 0xc2c4a
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2c4c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2c4f vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2c51
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2c54 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2c57
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2c5a vgabios.c:2040
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2c5d
    mov si, 00085h                            ; be 85 00                    ; 0xc2c60
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2c63
    cld                                       ; fc                          ; 0xc2c66
    jcxz 02c6fh                               ; e3 06                       ; 0xc2c67
    push DS                                   ; 1e                          ; 0xc2c69
    mov ds, dx                                ; 8e da                       ; 0xc2c6a
    rep movsb                                 ; f3 a4                       ; 0xc2c6c
    pop DS                                    ; 1f                          ; 0xc2c6e
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2c6f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c72
    mov es, ax                                ; 8e c0                       ; 0xc2c75
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2c77
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2c7a vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2c7d vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2c80
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2c83 vgabios.c:2043
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2c86 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2c8a vgabios.c:2044
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2c8d vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2c92 vgabios.c:2045
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2c95 vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2c99 vgabios.c:2046
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2c9c vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2ca0 vgabios.c:2047
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2ca3 vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2ca7 vgabios.c:2048
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2caa vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2cae vgabios.c:2049
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2cb1 vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2cb5 vgabios.c:2050
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2cb8 vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2cbc vgabios.c:2051
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2cbf vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2cc3 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cc6
    mov es, ax                                ; 8e c0                       ; 0xc2cc9
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2ccb
    mov ah, al                                ; 88 c4                       ; 0xc2cce vgabios.c:2056
    and ah, 080h                              ; 80 e4 80                    ; 0xc2cd0
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2cd3
    sar si, 006h                              ; c1 fe 06                    ; 0xc2cd6
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2cd9
    xor ah, ah                                ; 30 e4                       ; 0xc2cdb
    sar ax, 004h                              ; c1 f8 04                    ; 0xc2cdd
    or ax, si                                 ; 09 f0                       ; 0xc2ce0
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2ce2 vgabios.c:2057
    je short 02cf8h                           ; 74 11                       ; 0xc2ce5
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2ce7
    je short 02cf4h                           ; 74 08                       ; 0xc2cea
    test ax, ax                               ; 85 c0                       ; 0xc2cec
    jne short 02cf8h                          ; 75 08                       ; 0xc2cee
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2cf0 vgabios.c:2058
    jmp short 02cfah                          ; eb 06                       ; 0xc2cf2
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2cf4 vgabios.c:2059
    jmp short 02cfah                          ; eb 02                       ; 0xc2cf6
    xor al, al                                ; 30 c0                       ; 0xc2cf8 vgabios.c:2061
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2cfa vgabios.c:2063
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2cfd vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2d00
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d03 vgabios.c:2066
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2d06
    jc short 02d29h                           ; 72 1f                       ; 0xc2d08
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2d0a
    jnbe short 02d29h                         ; 77 1b                       ; 0xc2d0c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2d0e vgabios.c:2067
    test ax, ax                               ; 85 c0                       ; 0xc2d11
    je short 02d6bh                           ; 74 56                       ; 0xc2d13
    mov si, ax                                ; 89 c6                       ; 0xc2d15 vgabios.c:2068
    shr si, 002h                              ; c1 ee 02                    ; 0xc2d17
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2d1a
    xor dx, dx                                ; 31 d2                       ; 0xc2d1d
    div si                                    ; f7 f6                       ; 0xc2d1f
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2d21
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2d24 vgabios.c:42
    jmp short 02d6bh                          ; eb 42                       ; 0xc2d27 vgabios.c:2069
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2d29
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d2c
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2d2f
    jne short 02d44h                          ; 75 11                       ; 0xc2d31
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d33 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2d36
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d3a vgabios.c:2071
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2d3d vgabios.c:52
    jmp short 02d6bh                          ; eb 27                       ; 0xc2d42 vgabios.c:2072
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2d44
    jc short 02d6bh                           ; 72 23                       ; 0xc2d46
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2d48
    jnbe short 02d6bh                         ; 77 1f                       ; 0xc2d4a
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2d4c vgabios.c:2074
    je short 02d60h                           ; 74 0e                       ; 0xc2d50
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2d52 vgabios.c:2075
    xor dx, dx                                ; 31 d2                       ; 0xc2d55
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2d57
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d5a vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2d5d
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d60 vgabios.c:2076
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d63 vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2d66
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d6b vgabios.c:2078
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2d6e
    je short 02d76h                           ; 74 04                       ; 0xc2d70
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2d72
    jne short 02d81h                          ; 75 0b                       ; 0xc2d74
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d76 vgabios.c:2079
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d79 vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc2d7c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d81 vgabios.c:2081
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2d84
    jc short 02ddfh                           ; 72 57                       ; 0xc2d86
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc2d88
    je short 02ddfh                           ; 74 53                       ; 0xc2d8a
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2d8c vgabios.c:2082
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d8f vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2d92
    mov si, 00084h                            ; be 84 00                    ; 0xc2d96 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d99
    mov es, ax                                ; 8e c0                       ; 0xc2d9c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2d9e
    movzx di, al                              ; 0f b6 f8                    ; 0xc2da1 vgabios.c:38
    inc di                                    ; 47                          ; 0xc2da4
    mov si, 00085h                            ; be 85 00                    ; 0xc2da5 vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2da8
    xor ah, ah                                ; 30 e4                       ; 0xc2dab vgabios.c:38
    imul ax, di                               ; 0f af c7                    ; 0xc2dad
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc2db0 vgabios.c:2084
    jc short 02dc3h                           ; 72 0e                       ; 0xc2db3
    jbe short 02dcch                          ; 76 15                       ; 0xc2db5
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc2db7
    je short 02dd4h                           ; 74 18                       ; 0xc2dba
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc2dbc
    je short 02dd0h                           ; 74 0f                       ; 0xc2dbf
    jmp short 02dd4h                          ; eb 11                       ; 0xc2dc1
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc2dc3
    jne short 02dd4h                          ; 75 0c                       ; 0xc2dc6
    xor al, al                                ; 30 c0                       ; 0xc2dc8 vgabios.c:2085
    jmp short 02dd6h                          ; eb 0a                       ; 0xc2dca
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2dcc vgabios.c:2086
    jmp short 02dd6h                          ; eb 06                       ; 0xc2dce
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2dd0 vgabios.c:2087
    jmp short 02dd6h                          ; eb 02                       ; 0xc2dd2
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2dd4 vgabios.c:2089
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2dd6 vgabios.c:2091
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2dd9 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2ddc
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc2ddf vgabios.c:2094
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2de2
    xor ax, ax                                ; 31 c0                       ; 0xc2de5
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2de7
    cld                                       ; fc                          ; 0xc2dea
    jcxz 02defh                               ; e3 02                       ; 0xc2deb
    rep stosb                                 ; f3 aa                       ; 0xc2ded
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2def vgabios.c:2095
    pop di                                    ; 5f                          ; 0xc2df2
    pop si                                    ; 5e                          ; 0xc2df3
    pop cx                                    ; 59                          ; 0xc2df4
    pop bp                                    ; 5d                          ; 0xc2df5
    retn                                      ; c3                          ; 0xc2df6
  ; disGetNextSymbol 0xc2df7 LB 0x122e -> off=0x0 cb=0000000000000023 uValue=00000000000c2df7 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc2df7 LB 0x23
    push dx                                   ; 52                          ; 0xc2df7 vgabios.c:2098
    push bp                                   ; 55                          ; 0xc2df8
    mov bp, sp                                ; 89 e5                       ; 0xc2df9
    mov dx, ax                                ; 89 c2                       ; 0xc2dfb
    xor ax, ax                                ; 31 c0                       ; 0xc2dfd vgabios.c:2102
    test dl, 001h                             ; f6 c2 01                    ; 0xc2dff vgabios.c:2103
    je short 02e07h                           ; 74 03                       ; 0xc2e02
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2e04 vgabios.c:2104
    test dl, 002h                             ; f6 c2 02                    ; 0xc2e07 vgabios.c:2106
    je short 02e0fh                           ; 74 03                       ; 0xc2e0a
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2e0c vgabios.c:2107
    test dl, 004h                             ; f6 c2 04                    ; 0xc2e0f vgabios.c:2109
    je short 02e17h                           ; 74 03                       ; 0xc2e12
    add ax, 00304h                            ; 05 04 03                    ; 0xc2e14 vgabios.c:2110
    pop bp                                    ; 5d                          ; 0xc2e17 vgabios.c:2113
    pop dx                                    ; 5a                          ; 0xc2e18
    retn                                      ; c3                          ; 0xc2e19
  ; disGetNextSymbol 0xc2e1a LB 0x120b -> off=0x0 cb=0000000000000018 uValue=00000000000c2e1a 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc2e1a LB 0x18
    push bp                                   ; 55                          ; 0xc2e1a vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc2e1b
    push bx                                   ; 53                          ; 0xc2e1d
    mov bx, dx                                ; 89 d3                       ; 0xc2e1e
    call 02df7h                               ; e8 d4 ff                    ; 0xc2e20 vgabios.c:2118
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc2e23
    shr ax, 006h                              ; c1 e8 06                    ; 0xc2e26
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc2e29
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2e2c vgabios.c:2119
    pop bx                                    ; 5b                          ; 0xc2e2f
    pop bp                                    ; 5d                          ; 0xc2e30
    retn                                      ; c3                          ; 0xc2e31
  ; disGetNextSymbol 0xc2e32 LB 0x11f3 -> off=0x0 cb=00000000000002d6 uValue=00000000000c2e32 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc2e32 LB 0x2d6
    push bp                                   ; 55                          ; 0xc2e32 vgabios.c:2121
    mov bp, sp                                ; 89 e5                       ; 0xc2e33
    push cx                                   ; 51                          ; 0xc2e35
    push si                                   ; 56                          ; 0xc2e36
    push di                                   ; 57                          ; 0xc2e37
    push ax                                   ; 50                          ; 0xc2e38
    push ax                                   ; 50                          ; 0xc2e39
    push ax                                   ; 50                          ; 0xc2e3a
    mov cx, dx                                ; 89 d1                       ; 0xc2e3b
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2e3d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e40
    mov es, ax                                ; 8e c0                       ; 0xc2e43
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc2e45
    mov si, di                                ; 89 fe                       ; 0xc2e48 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc2e4a vgabios.c:2126
    je near 02f65h                            ; 0f 84 13 01                 ; 0xc2e4e
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e52 vgabios.c:2127
    in AL, DX                                 ; ec                          ; 0xc2e55
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e56
    mov es, cx                                ; 8e c1                       ; 0xc2e58 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e5a
    inc bx                                    ; 43                          ; 0xc2e5d vgabios.c:2127
    mov dx, di                                ; 89 fa                       ; 0xc2e5e
    in AL, DX                                 ; ec                          ; 0xc2e60
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e61
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e63 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2e66 vgabios.c:2128
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2e67
    in AL, DX                                 ; ec                          ; 0xc2e6a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e6b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e6d vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2e70 vgabios.c:2129
    mov dx, 003dah                            ; ba da 03                    ; 0xc2e71
    in AL, DX                                 ; ec                          ; 0xc2e74
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e75
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2e77 vgabios.c:2131
    in AL, DX                                 ; ec                          ; 0xc2e7a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e7b
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e7d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2e80 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e83
    inc bx                                    ; 43                          ; 0xc2e86 vgabios.c:2132
    mov dx, 003cah                            ; ba ca 03                    ; 0xc2e87
    in AL, DX                                 ; ec                          ; 0xc2e8a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e8b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e8d vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2e90 vgabios.c:2135
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2e93
    add bx, ax                                ; 01 c3                       ; 0xc2e96 vgabios.c:2133
    jmp short 02ea0h                          ; eb 06                       ; 0xc2e98
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc2e9a
    jnbe short 02eb8h                         ; 77 18                       ; 0xc2e9e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ea0 vgabios.c:2136
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2ea3
    out DX, AL                                ; ee                          ; 0xc2ea6
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2ea7 vgabios.c:2137
    in AL, DX                                 ; ec                          ; 0xc2eaa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2eab
    mov es, cx                                ; 8e c1                       ; 0xc2ead vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2eaf
    inc bx                                    ; 43                          ; 0xc2eb2 vgabios.c:2137
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2eb3 vgabios.c:2138
    jmp short 02e9ah                          ; eb e2                       ; 0xc2eb6
    xor al, al                                ; 30 c0                       ; 0xc2eb8 vgabios.c:2139
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2eba
    out DX, AL                                ; ee                          ; 0xc2ebd
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2ebe vgabios.c:2140
    in AL, DX                                 ; ec                          ; 0xc2ec1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ec2
    mov es, cx                                ; 8e c1                       ; 0xc2ec4 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ec6
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2ec9 vgabios.c:2142
    inc bx                                    ; 43                          ; 0xc2ece vgabios.c:2140
    jmp short 02ed7h                          ; eb 06                       ; 0xc2ecf
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc2ed1
    jnbe short 02eeeh                         ; 77 17                       ; 0xc2ed5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ed7 vgabios.c:2143
    mov dx, si                                ; 89 f2                       ; 0xc2eda
    out DX, AL                                ; ee                          ; 0xc2edc
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2edd vgabios.c:2144
    in AL, DX                                 ; ec                          ; 0xc2ee0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ee1
    mov es, cx                                ; 8e c1                       ; 0xc2ee3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ee5
    inc bx                                    ; 43                          ; 0xc2ee8 vgabios.c:2144
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2ee9 vgabios.c:2145
    jmp short 02ed1h                          ; eb e3                       ; 0xc2eec
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2eee vgabios.c:2147
    jmp short 02efbh                          ; eb 06                       ; 0xc2ef3
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc2ef5
    jnbe short 02f1fh                         ; 77 24                       ; 0xc2ef9
    mov dx, 003dah                            ; ba da 03                    ; 0xc2efb vgabios.c:2148
    in AL, DX                                 ; ec                          ; 0xc2efe
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2eff
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2f01 vgabios.c:2149
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2f04
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc2f07
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f0a
    out DX, AL                                ; ee                          ; 0xc2f0d
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc2f0e vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc2f11
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f12
    mov es, cx                                ; 8e c1                       ; 0xc2f14 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2f16
    inc bx                                    ; 43                          ; 0xc2f19 vgabios.c:2150
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2f1a vgabios.c:2151
    jmp short 02ef5h                          ; eb d6                       ; 0xc2f1d
    mov dx, 003dah                            ; ba da 03                    ; 0xc2f1f vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc2f22
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f23
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2f25 vgabios.c:2154
    jmp short 02f32h                          ; eb 06                       ; 0xc2f2a
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc2f2c
    jnbe short 02f4ah                         ; 77 18                       ; 0xc2f30
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f32 vgabios.c:2155
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2f35
    out DX, AL                                ; ee                          ; 0xc2f38
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2f39 vgabios.c:2156
    in AL, DX                                 ; ec                          ; 0xc2f3c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f3d
    mov es, cx                                ; 8e c1                       ; 0xc2f3f vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2f41
    inc bx                                    ; 43                          ; 0xc2f44 vgabios.c:2156
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2f45 vgabios.c:2157
    jmp short 02f2ch                          ; eb e2                       ; 0xc2f48
    mov es, cx                                ; 8e c1                       ; 0xc2f4a vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc2f4c
    inc bx                                    ; 43                          ; 0xc2f4f vgabios.c:2159
    inc bx                                    ; 43                          ; 0xc2f50
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f51 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f55 vgabios.c:2162
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f56 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f5a vgabios.c:2163
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f5b vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f5f vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f60 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f64 vgabios.c:2165
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc2f65 vgabios.c:2167
    je near 030ach                            ; 0f 84 3f 01                 ; 0xc2f69
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2f6d vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f70
    mov es, ax                                ; 8e c0                       ; 0xc2f73
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f75
    mov es, cx                                ; 8e c1                       ; 0xc2f78 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2f7a
    inc bx                                    ; 43                          ; 0xc2f7d vgabios.c:2168
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2f7e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f81
    mov es, ax                                ; 8e c0                       ; 0xc2f84
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f86
    mov es, cx                                ; 8e c1                       ; 0xc2f89 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f8b
    inc bx                                    ; 43                          ; 0xc2f8e vgabios.c:2169
    inc bx                                    ; 43                          ; 0xc2f8f
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2f90 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f93
    mov es, ax                                ; 8e c0                       ; 0xc2f96
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f98
    mov es, cx                                ; 8e c1                       ; 0xc2f9b vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f9d
    inc bx                                    ; 43                          ; 0xc2fa0 vgabios.c:2170
    inc bx                                    ; 43                          ; 0xc2fa1
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2fa2 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fa5
    mov es, ax                                ; 8e c0                       ; 0xc2fa8
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2faa
    mov es, cx                                ; 8e c1                       ; 0xc2fad vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2faf
    inc bx                                    ; 43                          ; 0xc2fb2 vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc2fb3
    mov si, 00084h                            ; be 84 00                    ; 0xc2fb4 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fb7
    mov es, ax                                ; 8e c0                       ; 0xc2fba
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fbc
    mov es, cx                                ; 8e c1                       ; 0xc2fbf vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fc1
    inc bx                                    ; 43                          ; 0xc2fc4 vgabios.c:2172
    mov si, 00085h                            ; be 85 00                    ; 0xc2fc5 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fc8
    mov es, ax                                ; 8e c0                       ; 0xc2fcb
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2fcd
    mov es, cx                                ; 8e c1                       ; 0xc2fd0 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2fd2
    inc bx                                    ; 43                          ; 0xc2fd5 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc2fd6
    mov si, 00087h                            ; be 87 00                    ; 0xc2fd7 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fda
    mov es, ax                                ; 8e c0                       ; 0xc2fdd
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fdf
    mov es, cx                                ; 8e c1                       ; 0xc2fe2 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fe4
    inc bx                                    ; 43                          ; 0xc2fe7 vgabios.c:2174
    mov si, 00088h                            ; be 88 00                    ; 0xc2fe8 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2feb
    mov es, ax                                ; 8e c0                       ; 0xc2fee
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2ff0
    mov es, cx                                ; 8e c1                       ; 0xc2ff3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ff5
    inc bx                                    ; 43                          ; 0xc2ff8 vgabios.c:2175
    mov si, 00089h                            ; be 89 00                    ; 0xc2ff9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ffc
    mov es, ax                                ; 8e c0                       ; 0xc2fff
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3001
    mov es, cx                                ; 8e c1                       ; 0xc3004 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3006
    inc bx                                    ; 43                          ; 0xc3009 vgabios.c:2176
    mov si, strict word 00060h                ; be 60 00                    ; 0xc300a vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc300d
    mov es, ax                                ; 8e c0                       ; 0xc3010
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3012
    mov es, cx                                ; 8e c1                       ; 0xc3015 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3017
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc301a vgabios.c:2178
    inc bx                                    ; 43                          ; 0xc301f vgabios.c:2177
    inc bx                                    ; 43                          ; 0xc3020
    jmp short 03029h                          ; eb 06                       ; 0xc3021
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3023
    jnc short 03045h                          ; 73 1c                       ; 0xc3027
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc3029 vgabios.c:2179
    add si, si                                ; 01 f6                       ; 0xc302c
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc302e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3031 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc3034
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3036
    mov es, cx                                ; 8e c1                       ; 0xc3039 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc303b
    inc bx                                    ; 43                          ; 0xc303e vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc303f
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3040 vgabios.c:2181
    jmp short 03023h                          ; eb de                       ; 0xc3043
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3045 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3048
    mov es, ax                                ; 8e c0                       ; 0xc304b
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc304d
    mov es, cx                                ; 8e c1                       ; 0xc3050 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3052
    inc bx                                    ; 43                          ; 0xc3055 vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc3056
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3057 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc305a
    mov es, ax                                ; 8e c0                       ; 0xc305d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc305f
    mov es, cx                                ; 8e c1                       ; 0xc3062 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3064
    inc bx                                    ; 43                          ; 0xc3067 vgabios.c:2183
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3068 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc306b
    mov es, ax                                ; 8e c0                       ; 0xc306d
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc306f
    mov es, cx                                ; 8e c1                       ; 0xc3072 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3074
    inc bx                                    ; 43                          ; 0xc3077 vgabios.c:2185
    inc bx                                    ; 43                          ; 0xc3078
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc3079 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc307c
    mov es, ax                                ; 8e c0                       ; 0xc307e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3080
    mov es, cx                                ; 8e c1                       ; 0xc3083 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3085
    inc bx                                    ; 43                          ; 0xc3088 vgabios.c:2186
    inc bx                                    ; 43                          ; 0xc3089
    mov si, 0010ch                            ; be 0c 01                    ; 0xc308a vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc308d
    mov es, ax                                ; 8e c0                       ; 0xc308f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3091
    mov es, cx                                ; 8e c1                       ; 0xc3094 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3096
    inc bx                                    ; 43                          ; 0xc3099 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc309a
    mov si, 0010eh                            ; be 0e 01                    ; 0xc309b vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc309e
    mov es, ax                                ; 8e c0                       ; 0xc30a0
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc30a2
    mov es, cx                                ; 8e c1                       ; 0xc30a5 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc30a7
    inc bx                                    ; 43                          ; 0xc30aa vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc30ab
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc30ac vgabios.c:2190
    je short 030feh                           ; 74 4c                       ; 0xc30b0
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc30b2 vgabios.c:2192
    in AL, DX                                 ; ec                          ; 0xc30b5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30b6
    mov es, cx                                ; 8e c1                       ; 0xc30b8 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ba
    inc bx                                    ; 43                          ; 0xc30bd vgabios.c:2192
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc30be
    in AL, DX                                 ; ec                          ; 0xc30c1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30c2
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30c4 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30c7 vgabios.c:2193
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc30c8
    in AL, DX                                 ; ec                          ; 0xc30cb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30cc
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ce vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30d1 vgabios.c:2194
    xor al, al                                ; 30 c0                       ; 0xc30d2
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc30d4
    out DX, AL                                ; ee                          ; 0xc30d7
    xor ah, ah                                ; 30 e4                       ; 0xc30d8 vgabios.c:2197
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc30da
    jmp short 030e6h                          ; eb 07                       ; 0xc30dd
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc30df
    jnc short 030f7h                          ; 73 11                       ; 0xc30e4
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc30e6 vgabios.c:2198
    in AL, DX                                 ; ec                          ; 0xc30e9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30ea
    mov es, cx                                ; 8e c1                       ; 0xc30ec vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ee
    inc bx                                    ; 43                          ; 0xc30f1 vgabios.c:2198
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30f2 vgabios.c:2199
    jmp short 030dfh                          ; eb e8                       ; 0xc30f5
    mov es, cx                                ; 8e c1                       ; 0xc30f7 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc30f9
    inc bx                                    ; 43                          ; 0xc30fd vgabios.c:2200
    mov ax, bx                                ; 89 d8                       ; 0xc30fe vgabios.c:2203
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3100
    pop di                                    ; 5f                          ; 0xc3103
    pop si                                    ; 5e                          ; 0xc3104
    pop cx                                    ; 59                          ; 0xc3105
    pop bp                                    ; 5d                          ; 0xc3106
    retn                                      ; c3                          ; 0xc3107
  ; disGetNextSymbol 0xc3108 LB 0xf1d -> off=0x0 cb=00000000000002b8 uValue=00000000000c3108 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3108 LB 0x2b8
    push bp                                   ; 55                          ; 0xc3108 vgabios.c:2205
    mov bp, sp                                ; 89 e5                       ; 0xc3109
    push cx                                   ; 51                          ; 0xc310b
    push si                                   ; 56                          ; 0xc310c
    push di                                   ; 57                          ; 0xc310d
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc310e
    push ax                                   ; 50                          ; 0xc3111
    mov cx, dx                                ; 89 d1                       ; 0xc3112
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3114 vgabios.c:2209
    je near 03250h                            ; 0f 84 34 01                 ; 0xc3118
    mov dx, 003dah                            ; ba da 03                    ; 0xc311c vgabios.c:2211
    in AL, DX                                 ; ec                          ; 0xc311f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3120
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc3122 vgabios.c:2213
    mov es, cx                                ; 8e c1                       ; 0xc3125 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3127
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc312a vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc312d vgabios.c:2214
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc312f vgabios.c:2217
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc3134 vgabios.c:2215
    jmp short 0313fh                          ; eb 06                       ; 0xc3137
    cmp word [bp-00eh], strict byte 00004h    ; 83 7e f2 04                 ; 0xc3139
    jnbe short 03155h                         ; 77 16                       ; 0xc313d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc313f vgabios.c:2218
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3142
    out DX, AL                                ; ee                          ; 0xc3145
    mov es, cx                                ; 8e c1                       ; 0xc3146 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3148
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc314b vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc314e
    inc bx                                    ; 43                          ; 0xc314f vgabios.c:2219
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3150 vgabios.c:2220
    jmp short 03139h                          ; eb e4                       ; 0xc3153
    xor al, al                                ; 30 c0                       ; 0xc3155 vgabios.c:2221
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3157
    out DX, AL                                ; ee                          ; 0xc315a
    mov es, cx                                ; 8e c1                       ; 0xc315b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc315d
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3160 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3163
    inc bx                                    ; 43                          ; 0xc3164 vgabios.c:2222
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3165
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3168
    out DX, ax                                ; ef                          ; 0xc316b
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc316c vgabios.c:2227
    jmp short 03179h                          ; eb 06                       ; 0xc3171
    cmp word [bp-00eh], strict byte 00018h    ; 83 7e f2 18                 ; 0xc3173
    jnbe short 03193h                         ; 77 1a                       ; 0xc3177
    cmp word [bp-00eh], strict byte 00011h    ; 83 7e f2 11                 ; 0xc3179 vgabios.c:2228
    je short 0318dh                           ; 74 0e                       ; 0xc317d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc317f vgabios.c:2229
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3182
    out DX, AL                                ; ee                          ; 0xc3185
    mov es, cx                                ; 8e c1                       ; 0xc3186 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3188
    inc dx                                    ; 42                          ; 0xc318b vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc318c
    inc bx                                    ; 43                          ; 0xc318d vgabios.c:2232
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc318e vgabios.c:2233
    jmp short 03173h                          ; eb e0                       ; 0xc3191
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3193 vgabios.c:2235
    in AL, DX                                 ; ec                          ; 0xc3196
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3197
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3199
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc319b
    cmp word [bp-00ah], 003d4h                ; 81 7e f6 d4 03              ; 0xc319e vgabios.c:2236
    jne short 031a9h                          ; 75 04                       ; 0xc31a3
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc31a5 vgabios.c:2237
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc31a9 vgabios.c:2238
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc31ac
    out DX, AL                                ; ee                          ; 0xc31af
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc31b0 vgabios.c:2241
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc31b2
    out DX, AL                                ; ee                          ; 0xc31b5
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc31b6 vgabios.c:2242
    mov es, cx                                ; 8e c1                       ; 0xc31ba vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc31bc
    inc dx                                    ; 42                          ; 0xc31bf vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc31c0
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc31c1 vgabios.c:2245
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc31c4 vgabios.c:37
    xor ah, ah                                ; 30 e4                       ; 0xc31c7 vgabios.c:38
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc31c9
    mov dx, 003dah                            ; ba da 03                    ; 0xc31cc vgabios.c:2246
    in AL, DX                                 ; ec                          ; 0xc31cf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31d0
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc31d2 vgabios.c:2247
    jmp short 031dfh                          ; eb 06                       ; 0xc31d7
    cmp word [bp-00eh], strict byte 00013h    ; 83 7e f2 13                 ; 0xc31d9
    jnbe short 031f8h                         ; 77 19                       ; 0xc31dd
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc31df vgabios.c:2248
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc31e2
    or ax, word [bp-00eh]                     ; 0b 46 f2                    ; 0xc31e5
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc31e8
    out DX, AL                                ; ee                          ; 0xc31eb
    mov es, cx                                ; 8e c1                       ; 0xc31ec vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc31ee
    out DX, AL                                ; ee                          ; 0xc31f1 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc31f2 vgabios.c:2249
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc31f3 vgabios.c:2250
    jmp short 031d9h                          ; eb e1                       ; 0xc31f6
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc31f8 vgabios.c:2251
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc31fb
    out DX, AL                                ; ee                          ; 0xc31fe
    mov dx, 003dah                            ; ba da 03                    ; 0xc31ff vgabios.c:2252
    in AL, DX                                 ; ec                          ; 0xc3202
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3203
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3205 vgabios.c:2254
    jmp short 03212h                          ; eb 06                       ; 0xc320a
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc320c
    jnbe short 03228h                         ; 77 16                       ; 0xc3210
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3212 vgabios.c:2255
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3215
    out DX, AL                                ; ee                          ; 0xc3218
    mov es, cx                                ; 8e c1                       ; 0xc3219 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc321b
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc321e vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3221
    inc bx                                    ; 43                          ; 0xc3222 vgabios.c:2256
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3223 vgabios.c:2257
    jmp short 0320ch                          ; eb e4                       ; 0xc3226
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc3228 vgabios.c:2258
    mov es, cx                                ; 8e c1                       ; 0xc322b vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc322d
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3230 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3233
    inc si                                    ; 46                          ; 0xc3234 vgabios.c:2261
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3235 vgabios.c:37
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3238 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc323b
    inc si                                    ; 46                          ; 0xc323c vgabios.c:2262
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc323d vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3240 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3243
    inc si                                    ; 46                          ; 0xc3244 vgabios.c:2263
    inc si                                    ; 46                          ; 0xc3245
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3246 vgabios.c:37
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3249 vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc324c
    out DX, AL                                ; ee                          ; 0xc324f
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3250 vgabios.c:2267
    je near 03373h                            ; 0f 84 1b 01                 ; 0xc3254
    mov es, cx                                ; 8e c1                       ; 0xc3258 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc325a
    mov si, strict word 00049h                ; be 49 00                    ; 0xc325d vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3260
    mov es, dx                                ; 8e c2                       ; 0xc3263
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3265
    inc bx                                    ; 43                          ; 0xc3268 vgabios.c:2268
    mov es, cx                                ; 8e c1                       ; 0xc3269 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc326b
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc326e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3271
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3273
    inc bx                                    ; 43                          ; 0xc3276 vgabios.c:2269
    inc bx                                    ; 43                          ; 0xc3277
    mov es, cx                                ; 8e c1                       ; 0xc3278 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc327a
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc327d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3280
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3282
    inc bx                                    ; 43                          ; 0xc3285 vgabios.c:2270
    inc bx                                    ; 43                          ; 0xc3286
    mov es, cx                                ; 8e c1                       ; 0xc3287 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3289
    mov si, strict word 00063h                ; be 63 00                    ; 0xc328c vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc328f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3291
    inc bx                                    ; 43                          ; 0xc3294 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc3295
    mov es, cx                                ; 8e c1                       ; 0xc3296 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3298
    mov si, 00084h                            ; be 84 00                    ; 0xc329b vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc329e
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32a0
    inc bx                                    ; 43                          ; 0xc32a3 vgabios.c:2272
    mov es, cx                                ; 8e c1                       ; 0xc32a4 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32a6
    mov si, 00085h                            ; be 85 00                    ; 0xc32a9 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc32ac
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc32ae
    inc bx                                    ; 43                          ; 0xc32b1 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc32b2
    mov es, cx                                ; 8e c1                       ; 0xc32b3 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc32b5
    mov si, 00087h                            ; be 87 00                    ; 0xc32b8 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc32bb
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32bd
    inc bx                                    ; 43                          ; 0xc32c0 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc32c1 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc32c3
    mov si, 00088h                            ; be 88 00                    ; 0xc32c6 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc32c9
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32cb
    inc bx                                    ; 43                          ; 0xc32ce vgabios.c:2275
    mov es, cx                                ; 8e c1                       ; 0xc32cf vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc32d1
    mov si, 00089h                            ; be 89 00                    ; 0xc32d4 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc32d7
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32d9
    inc bx                                    ; 43                          ; 0xc32dc vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc32dd vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32df
    mov si, strict word 00060h                ; be 60 00                    ; 0xc32e2 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc32e5
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc32e7
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc32ea vgabios.c:2278
    inc bx                                    ; 43                          ; 0xc32ef vgabios.c:2277
    inc bx                                    ; 43                          ; 0xc32f0
    jmp short 032f9h                          ; eb 06                       ; 0xc32f1
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc32f3
    jnc short 03315h                          ; 73 1c                       ; 0xc32f7
    mov es, cx                                ; 8e c1                       ; 0xc32f9 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32fb
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc32fe vgabios.c:48
    add si, si                                ; 01 f6                       ; 0xc3301
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3303
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3306 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3309
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc330b
    inc bx                                    ; 43                          ; 0xc330e vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc330f
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3310 vgabios.c:2281
    jmp short 032f3h                          ; eb de                       ; 0xc3313
    mov es, cx                                ; 8e c1                       ; 0xc3315 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3317
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc331a vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc331d
    mov es, dx                                ; 8e c2                       ; 0xc3320
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3322
    inc bx                                    ; 43                          ; 0xc3325 vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc3326
    mov es, cx                                ; 8e c1                       ; 0xc3327 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3329
    mov si, strict word 00062h                ; be 62 00                    ; 0xc332c vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc332f
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3331
    inc bx                                    ; 43                          ; 0xc3334 vgabios.c:2283
    mov es, cx                                ; 8e c1                       ; 0xc3335 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3337
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc333a vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc333d
    mov es, dx                                ; 8e c2                       ; 0xc333f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3341
    inc bx                                    ; 43                          ; 0xc3344 vgabios.c:2285
    inc bx                                    ; 43                          ; 0xc3345
    mov es, cx                                ; 8e c1                       ; 0xc3346 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3348
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc334b vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc334e
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3350
    inc bx                                    ; 43                          ; 0xc3353 vgabios.c:2286
    inc bx                                    ; 43                          ; 0xc3354
    mov es, cx                                ; 8e c1                       ; 0xc3355 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3357
    mov si, 0010ch                            ; be 0c 01                    ; 0xc335a vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc335d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc335f
    inc bx                                    ; 43                          ; 0xc3362 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc3363
    mov es, cx                                ; 8e c1                       ; 0xc3364 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3366
    mov si, 0010eh                            ; be 0e 01                    ; 0xc3369 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc336c
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc336e
    inc bx                                    ; 43                          ; 0xc3371 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc3372
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3373 vgabios.c:2290
    je short 033b6h                           ; 74 3d                       ; 0xc3377
    inc bx                                    ; 43                          ; 0xc3379 vgabios.c:2291
    mov es, cx                                ; 8e c1                       ; 0xc337a vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc337c
    xor ah, ah                                ; 30 e4                       ; 0xc337f vgabios.c:38
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3381
    inc bx                                    ; 43                          ; 0xc3384 vgabios.c:2292
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3385 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3388 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc338b
    inc bx                                    ; 43                          ; 0xc338c vgabios.c:2293
    xor al, al                                ; 30 c0                       ; 0xc338d
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc338f
    out DX, AL                                ; ee                          ; 0xc3392
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3393 vgabios.c:2296
    jmp short 0339fh                          ; eb 07                       ; 0xc3396
    cmp word [bp-00eh], 00300h                ; 81 7e f2 00 03              ; 0xc3398
    jnc short 033aeh                          ; 73 0f                       ; 0xc339d
    mov es, cx                                ; 8e c1                       ; 0xc339f vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33a1
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc33a4 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33a7
    inc bx                                    ; 43                          ; 0xc33a8 vgabios.c:2297
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc33a9 vgabios.c:2298
    jmp short 03398h                          ; eb ea                       ; 0xc33ac
    inc bx                                    ; 43                          ; 0xc33ae vgabios.c:2299
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc33af
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc33b2
    out DX, AL                                ; ee                          ; 0xc33b5
    mov ax, bx                                ; 89 d8                       ; 0xc33b6 vgabios.c:2303
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc33b8
    pop di                                    ; 5f                          ; 0xc33bb
    pop si                                    ; 5e                          ; 0xc33bc
    pop cx                                    ; 59                          ; 0xc33bd
    pop bp                                    ; 5d                          ; 0xc33be
    retn                                      ; c3                          ; 0xc33bf
  ; disGetNextSymbol 0xc33c0 LB 0xc65 -> off=0x0 cb=0000000000000027 uValue=00000000000c33c0 'find_vga_entry'
find_vga_entry:                              ; 0xc33c0 LB 0x27
    push bx                                   ; 53                          ; 0xc33c0 vgabios.c:2312
    push dx                                   ; 52                          ; 0xc33c1
    push bp                                   ; 55                          ; 0xc33c2
    mov bp, sp                                ; 89 e5                       ; 0xc33c3
    mov dl, al                                ; 88 c2                       ; 0xc33c5
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc33c7 vgabios.c:2314
    xor al, al                                ; 30 c0                       ; 0xc33c9 vgabios.c:2315
    jmp short 033d3h                          ; eb 06                       ; 0xc33cb
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc33cd vgabios.c:2316
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc33cf
    jnbe short 033e1h                         ; 77 0e                       ; 0xc33d1
    movzx bx, al                              ; 0f b6 d8                    ; 0xc33d3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc33d6
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc33d9
    jne short 033cdh                          ; 75 ee                       ; 0xc33dd
    mov ah, al                                ; 88 c4                       ; 0xc33df
    mov al, ah                                ; 88 e0                       ; 0xc33e1 vgabios.c:2321
    pop bp                                    ; 5d                          ; 0xc33e3
    pop dx                                    ; 5a                          ; 0xc33e4
    pop bx                                    ; 5b                          ; 0xc33e5
    retn                                      ; c3                          ; 0xc33e6
  ; disGetNextSymbol 0xc33e7 LB 0xc3e -> off=0x0 cb=000000000000000e uValue=00000000000c33e7 'xread_byte'
xread_byte:                                  ; 0xc33e7 LB 0xe
    push bx                                   ; 53                          ; 0xc33e7 vgabios.c:2333
    push bp                                   ; 55                          ; 0xc33e8
    mov bp, sp                                ; 89 e5                       ; 0xc33e9
    mov bx, dx                                ; 89 d3                       ; 0xc33eb
    mov es, ax                                ; 8e c0                       ; 0xc33ed vgabios.c:2335
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33ef
    pop bp                                    ; 5d                          ; 0xc33f2 vgabios.c:2336
    pop bx                                    ; 5b                          ; 0xc33f3
    retn                                      ; c3                          ; 0xc33f4
  ; disGetNextSymbol 0xc33f5 LB 0xc30 -> off=0x87 cb=000000000000042e uValue=00000000000c347c 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0a3h, 038h, 0a7h, 034h, 0e4h, 034h, 0f8h, 034h, 009h, 035h
    db  01dh, 035h, 02eh, 035h, 039h, 035h, 073h, 035h, 077h, 035h, 088h, 035h, 0a5h, 035h, 0c2h, 035h
    db  0e2h, 035h, 0ffh, 035h, 016h, 036h, 022h, 036h, 0f2h, 036h, 066h, 037h, 093h, 037h, 0a8h, 037h
    db  0eah, 037h, 075h, 038h, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 0a3h, 038h, 043h, 036h, 067h, 036h, 075h, 036h, 083h, 036h, 043h, 036h, 067h, 036h
    db  075h, 036h, 083h, 036h, 091h, 036h, 09dh, 036h, 0b8h, 036h, 0c3h, 036h, 0ceh, 036h, 0d9h, 036h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 067h, 038h, 012h, 038h, 020h, 038h, 031h, 038h, 041h
    db  038h, 056h, 038h, 067h, 038h, 067h, 038h
int10_func:                                  ; 0xc347c LB 0x42e
    push bp                                   ; 55                          ; 0xc347c vgabios.c:2414
    mov bp, sp                                ; 89 e5                       ; 0xc347d
    push si                                   ; 56                          ; 0xc347f
    push di                                   ; 57                          ; 0xc3480
    push ax                                   ; 50                          ; 0xc3481
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3482
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3485 vgabios.c:2419
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3488
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc348b
    jnbe near 038a3h                          ; 0f 87 11 04                 ; 0xc348e
    push CS                                   ; 0e                          ; 0xc3492
    pop ES                                    ; 07                          ; 0xc3493
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3494
    mov di, 033f5h                            ; bf f5 33                    ; 0xc3497
    repne scasb                               ; f2 ae                       ; 0xc349a
    sal cx, 1                                 ; d1 e1                       ; 0xc349c
    mov di, cx                                ; 89 cf                       ; 0xc349e
    mov ax, word [cs:di+0340bh]               ; 2e 8b 85 0b 34              ; 0xc34a0
    jmp ax                                    ; ff e0                       ; 0xc34a5
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc34a7 vgabios.c:2422
    call 0130eh                               ; e8 60 de                    ; 0xc34ab
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34ae vgabios.c:2423
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc34b1
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc34b4
    je short 034ceh                           ; 74 15                       ; 0xc34b7
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc34b9
    je short 034c5h                           ; 74 07                       ; 0xc34bc
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc34be
    jbe short 034ceh                          ; 76 0b                       ; 0xc34c1
    jmp short 034d7h                          ; eb 12                       ; 0xc34c3
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34c5 vgabios.c:2425
    xor al, al                                ; 30 c0                       ; 0xc34c8
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc34ca
    jmp short 034deh                          ; eb 10                       ; 0xc34cc vgabios.c:2426
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34ce vgabios.c:2434
    xor al, al                                ; 30 c0                       ; 0xc34d1
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc34d3
    jmp short 034deh                          ; eb 07                       ; 0xc34d5
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34d7 vgabios.c:2437
    xor al, al                                ; 30 c0                       ; 0xc34da
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc34dc
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc34de
    jmp near 038a3h                           ; e9 bf 03                    ; 0xc34e1 vgabios.c:2439
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc34e4 vgabios.c:2441
    movzx dx, al                              ; 0f b6 d0                    ; 0xc34e7
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc34ea
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34ed
    xor ah, ah                                ; 30 e4                       ; 0xc34f0
    call 010c2h                               ; e8 cd db                    ; 0xc34f2
    jmp near 038a3h                           ; e9 ab 03                    ; 0xc34f5 vgabios.c:2442
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc34f8 vgabios.c:2444
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34fb
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34fe
    xor ah, ah                                ; 30 e4                       ; 0xc3501
    call 011b8h                               ; e8 b2 dc                    ; 0xc3503
    jmp near 038a3h                           ; e9 9a 03                    ; 0xc3506 vgabios.c:2445
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3509 vgabios.c:2447
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc350c
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc350f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3512
    xor ah, ah                                ; 30 e4                       ; 0xc3515
    call 00a08h                               ; e8 ee d4                    ; 0xc3517
    jmp near 038a3h                           ; e9 86 03                    ; 0xc351a vgabios.c:2448
    xor ax, ax                                ; 31 c0                       ; 0xc351d vgabios.c:2454
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc351f
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3522 vgabios.c:2455
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3525 vgabios.c:2456
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3528 vgabios.c:2457
    jmp near 038a3h                           ; e9 75 03                    ; 0xc352b vgabios.c:2458
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc352e vgabios.c:2460
    xor ah, ah                                ; 30 e4                       ; 0xc3531
    call 01241h                               ; e8 0b dd                    ; 0xc3533
    jmp near 038a3h                           ; e9 6a 03                    ; 0xc3536 vgabios.c:2461
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3539 vgabios.c:2463
    push ax                                   ; 50                          ; 0xc353c
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc353d
    push ax                                   ; 50                          ; 0xc3540
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3541
    xor ah, ah                                ; 30 e4                       ; 0xc3544
    push ax                                   ; 50                          ; 0xc3546
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3547
    shr ax, 008h                              ; c1 e8 08                    ; 0xc354a
    xor ah, ah                                ; 30 e4                       ; 0xc354d
    push ax                                   ; 50                          ; 0xc354f
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3550
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3553
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3556
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3559
    movzx bx, al                              ; 0f b6 d8                    ; 0xc355c
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc355f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3562
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3565
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3568
    xor ah, ah                                ; 30 e4                       ; 0xc356b
    call 0194dh                               ; e8 dd e3                    ; 0xc356d
    jmp near 038a3h                           ; e9 30 03                    ; 0xc3570 vgabios.c:2464
    xor ax, ax                                ; 31 c0                       ; 0xc3573 vgabios.c:2466
    jmp short 0353ch                          ; eb c5                       ; 0xc3575
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3577 vgabios.c:2469
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc357a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc357d
    xor ah, ah                                ; 30 e4                       ; 0xc3580
    call 00d25h                               ; e8 a0 d7                    ; 0xc3582
    jmp near 038a3h                           ; e9 1b 03                    ; 0xc3585 vgabios.c:2470
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3588 vgabios.c:2472
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc358b
    movzx bx, al                              ; 0f b6 d8                    ; 0xc358e
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3591
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3594
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3597
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc359a
    xor ah, ah                                ; 30 e4                       ; 0xc359d
    call 021deh                               ; e8 3c ec                    ; 0xc359f
    jmp near 038a3h                           ; e9 fe 02                    ; 0xc35a2 vgabios.c:2473
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc35a5 vgabios.c:2475
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc35a8
    movzx bx, al                              ; 0f b6 d8                    ; 0xc35ab
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35ae
    shr ax, 008h                              ; c1 e8 08                    ; 0xc35b1
    movzx dx, al                              ; 0f b6 d0                    ; 0xc35b4
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc35b7
    xor ah, ah                                ; 30 e4                       ; 0xc35ba
    call 02344h                               ; e8 85 ed                    ; 0xc35bc
    jmp near 038a3h                           ; e9 e1 02                    ; 0xc35bf vgabios.c:2476
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc35c2 vgabios.c:2478
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc35c5
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc35c8
    movzx dx, al                              ; 0f b6 d0                    ; 0xc35cb
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35ce
    shr ax, 008h                              ; c1 e8 08                    ; 0xc35d1
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc35d4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc35d7
    xor ah, ah                                ; 30 e4                       ; 0xc35da
    call 024a6h                               ; e8 c7 ee                    ; 0xc35dc
    jmp near 038a3h                           ; e9 c1 02                    ; 0xc35df vgabios.c:2479
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc35e2 vgabios.c:2481
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc35e5
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35e8
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35eb
    shr ax, 008h                              ; c1 e8 08                    ; 0xc35ee
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc35f1
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc35f4
    xor ah, ah                                ; 30 e4                       ; 0xc35f7
    call 00ee0h                               ; e8 e4 d8                    ; 0xc35f9
    jmp near 038a3h                           ; e9 a4 02                    ; 0xc35fc vgabios.c:2482
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc35ff vgabios.c:2490
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3602
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3605
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc3608
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc360b
    xor ah, ah                                ; 30 e4                       ; 0xc360e
    call 0260bh                               ; e8 f8 ef                    ; 0xc3610
    jmp near 038a3h                           ; e9 8d 02                    ; 0xc3613 vgabios.c:2491
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3616 vgabios.c:2494
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3619
    call 01036h                               ; e8 17 da                    ; 0xc361c
    jmp near 038a3h                           ; e9 81 02                    ; 0xc361f vgabios.c:2495
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3622 vgabios.c:2497
    xor ah, ah                                ; 30 e4                       ; 0xc3625
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3627
    jnbe near 038a3h                          ; 0f 87 75 02                 ; 0xc362a
    push CS                                   ; 0e                          ; 0xc362e
    pop ES                                    ; 07                          ; 0xc362f
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc3630
    mov di, 03439h                            ; bf 39 34                    ; 0xc3633
    repne scasb                               ; f2 ae                       ; 0xc3636
    sal cx, 1                                 ; d1 e1                       ; 0xc3638
    mov di, cx                                ; 89 cf                       ; 0xc363a
    mov ax, word [cs:di+03447h]               ; 2e 8b 85 47 34              ; 0xc363c
    jmp ax                                    ; ff e0                       ; 0xc3641
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3643 vgabios.c:2501
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3646
    xor ah, ah                                ; 30 e4                       ; 0xc3649
    push ax                                   ; 50                          ; 0xc364b
    movzx ax, byte [bp+00ch]                  ; 0f b6 46 0c                 ; 0xc364c
    push ax                                   ; 50                          ; 0xc3650
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3651
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3654
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3658
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc365b
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc365e
    call 02971h                               ; e8 0d f3                    ; 0xc3661
    jmp near 038a3h                           ; e9 3c 02                    ; 0xc3664 vgabios.c:2502
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3667 vgabios.c:2505
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc366b
    call 029eeh                               ; e8 7c f3                    ; 0xc366f
    jmp near 038a3h                           ; e9 2e 02                    ; 0xc3672 vgabios.c:2506
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3675 vgabios.c:2509
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3679
    call 02a5eh                               ; e8 de f3                    ; 0xc367d
    jmp near 038a3h                           ; e9 20 02                    ; 0xc3680 vgabios.c:2510
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3683 vgabios.c:2513
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3687
    call 02ad0h                               ; e8 42 f4                    ; 0xc368b
    jmp near 038a3h                           ; e9 12 02                    ; 0xc368e vgabios.c:2514
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3691 vgabios.c:2516
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3694
    call 02b42h                               ; e8 a8 f4                    ; 0xc3697
    jmp near 038a3h                           ; e9 06 02                    ; 0xc369a vgabios.c:2517
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc369d vgabios.c:2519
    xor ah, ah                                ; 30 e4                       ; 0xc36a0
    push ax                                   ; 50                          ; 0xc36a2
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc36a3
    movzx cx, al                              ; 0f b6 c8                    ; 0xc36a6
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc36a9
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc36ac
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc36af
    call 02b47h                               ; e8 92 f4                    ; 0xc36b2
    jmp near 038a3h                           ; e9 eb 01                    ; 0xc36b5 vgabios.c:2520
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc36b8 vgabios.c:2522
    xor ah, ah                                ; 30 e4                       ; 0xc36bb
    call 02b4eh                               ; e8 8e f4                    ; 0xc36bd
    jmp near 038a3h                           ; e9 e0 01                    ; 0xc36c0 vgabios.c:2523
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc36c3 vgabios.c:2525
    xor ah, ah                                ; 30 e4                       ; 0xc36c6
    call 02b53h                               ; e8 88 f4                    ; 0xc36c8
    jmp near 038a3h                           ; e9 d5 01                    ; 0xc36cb vgabios.c:2526
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc36ce vgabios.c:2528
    xor ah, ah                                ; 30 e4                       ; 0xc36d1
    call 02b58h                               ; e8 82 f4                    ; 0xc36d3
    jmp near 038a3h                           ; e9 ca 01                    ; 0xc36d6 vgabios.c:2529
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc36d9 vgabios.c:2531
    push ax                                   ; 50                          ; 0xc36dc
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc36dd
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc36e0
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc36e3
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc36e6
    shr ax, 008h                              ; c1 e8 08                    ; 0xc36e9
    call 00e5ch                               ; e8 6d d7                    ; 0xc36ec
    jmp near 038a3h                           ; e9 b1 01                    ; 0xc36ef vgabios.c:2539
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc36f2 vgabios.c:2541
    xor ah, ah                                ; 30 e4                       ; 0xc36f5
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc36f7
    jc short 0370bh                           ; 72 0f                       ; 0xc36fa
    jbe short 03718h                          ; 76 1a                       ; 0xc36fc
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc36fe
    je short 0375ch                           ; 74 59                       ; 0xc3701
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3703
    je short 0374dh                           ; 74 45                       ; 0xc3706
    jmp near 038a3h                           ; e9 98 01                    ; 0xc3708
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc370b
    jne near 038a3h                           ; 0f 85 91 01                 ; 0xc370e
    call 02b5dh                               ; e8 48 f4                    ; 0xc3712 vgabios.c:2544
    jmp near 038a3h                           ; e9 8b 01                    ; 0xc3715 vgabios.c:2545
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3718 vgabios.c:2547
    xor ah, ah                                ; 30 e4                       ; 0xc371b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc371d
    jnc short 03747h                          ; 73 25                       ; 0xc3720
    mov dx, 00087h                            ; ba 87 00                    ; 0xc3722 vgabios.c:2548
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3725
    call 033e7h                               ; e8 bc fc                    ; 0xc3728
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc372b
    mov ah, byte [bp+012h]                    ; 8a 66 12                    ; 0xc372d
    or al, ah                                 ; 08 e0                       ; 0xc3730
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3732 vgabios.c:40
    mov si, 00087h                            ; be 87 00                    ; 0xc3735
    mov es, dx                                ; 8e c2                       ; 0xc3738 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc373a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc373d vgabios.c:2550
    xor al, al                                ; 30 c0                       ; 0xc3740
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3742
    jmp near 034deh                           ; e9 97 fd                    ; 0xc3744
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc3747 vgabios.c:2553
    jmp near 038a3h                           ; e9 56 01                    ; 0xc374a vgabios.c:2554
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc374d vgabios.c:2556
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3751
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3754
    call 02b62h                               ; e8 08 f4                    ; 0xc3757
    jmp short 0373dh                          ; eb e1                       ; 0xc375a
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc375c vgabios.c:2560
    xor ah, ah                                ; 30 e4                       ; 0xc375f
    call 02b67h                               ; e8 03 f4                    ; 0xc3761
    jmp short 0373dh                          ; eb d7                       ; 0xc3764
    push word [bp+008h]                       ; ff 76 08                    ; 0xc3766 vgabios.c:2570
    push word [bp+016h]                       ; ff 76 16                    ; 0xc3769
    movzx ax, byte [bp+00eh]                  ; 0f b6 46 0e                 ; 0xc376c
    push ax                                   ; 50                          ; 0xc3770
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3771
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3774
    xor ah, ah                                ; 30 e4                       ; 0xc3777
    push ax                                   ; 50                          ; 0xc3779
    movzx bx, byte [bp+00ch]                  ; 0f b6 5e 0c                 ; 0xc377a
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc377e
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3781
    xor dh, dh                                ; 30 f6                       ; 0xc3784
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3786
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc378a
    call 02b6ch                               ; e8 dc f3                    ; 0xc378d
    jmp near 038a3h                           ; e9 10 01                    ; 0xc3790 vgabios.c:2571
    mov bx, si                                ; 89 f3                       ; 0xc3793 vgabios.c:2573
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3795
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3798
    call 02c02h                               ; e8 64 f4                    ; 0xc379b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc379e vgabios.c:2574
    xor al, al                                ; 30 c0                       ; 0xc37a1
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc37a3
    jmp near 034deh                           ; e9 36 fd                    ; 0xc37a5
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37a8 vgabios.c:2577
    xor ah, ah                                ; 30 e4                       ; 0xc37ab
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc37ad
    je short 037d4h                           ; 74 22                       ; 0xc37b0
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc37b2
    je short 037c6h                           ; 74 0f                       ; 0xc37b5
    test ax, ax                               ; 85 c0                       ; 0xc37b7
    jne short 037e0h                          ; 75 25                       ; 0xc37b9
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc37bb vgabios.c:2580
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37be
    call 02e1ah                               ; e8 56 f6                    ; 0xc37c1
    jmp short 037e0h                          ; eb 1a                       ; 0xc37c4 vgabios.c:2581
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc37c6 vgabios.c:2583
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc37c9
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37cc
    call 02e32h                               ; e8 60 f6                    ; 0xc37cf
    jmp short 037e0h                          ; eb 0c                       ; 0xc37d2 vgabios.c:2584
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc37d4 vgabios.c:2586
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc37d7
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37da
    call 03108h                               ; e8 28 f9                    ; 0xc37dd
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37e0 vgabios.c:2593
    xor al, al                                ; 30 c0                       ; 0xc37e3
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc37e5
    jmp near 034deh                           ; e9 f4 fc                    ; 0xc37e7
    call 007bfh                               ; e8 d2 cf                    ; 0xc37ea vgabios.c:2598
    test ax, ax                               ; 85 c0                       ; 0xc37ed
    je near 0386eh                            ; 0f 84 7b 00                 ; 0xc37ef
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37f3 vgabios.c:2599
    xor ah, ah                                ; 30 e4                       ; 0xc37f6
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc37f8
    jnbe short 03867h                         ; 77 6a                       ; 0xc37fb
    push CS                                   ; 0e                          ; 0xc37fd
    pop ES                                    ; 07                          ; 0xc37fe
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc37ff
    mov di, 03465h                            ; bf 65 34                    ; 0xc3802
    repne scasb                               ; f2 ae                       ; 0xc3805
    sal cx, 1                                 ; d1 e1                       ; 0xc3807
    mov di, cx                                ; 89 cf                       ; 0xc3809
    mov ax, word [cs:di+0346ch]               ; 2e 8b 85 6c 34              ; 0xc380b
    jmp ax                                    ; ff e0                       ; 0xc3810
    mov bx, si                                ; 89 f3                       ; 0xc3812 vgabios.c:2602
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3814
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3817
    call 03a5dh                               ; e8 40 02                    ; 0xc381a
    jmp near 038a3h                           ; e9 83 00                    ; 0xc381d vgabios.c:2603
    mov cx, si                                ; 89 f1                       ; 0xc3820 vgabios.c:2605
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3822
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3825
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3828
    call 03b82h                               ; e8 54 03                    ; 0xc382b
    jmp near 038a3h                           ; e9 72 00                    ; 0xc382e vgabios.c:2606
    mov cx, si                                ; 89 f1                       ; 0xc3831 vgabios.c:2608
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3833
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3836
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3839
    call 03c1eh                               ; e8 df 03                    ; 0xc383c
    jmp short 038a3h                          ; eb 62                       ; 0xc383f vgabios.c:2609
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3841 vgabios.c:2611
    push ax                                   ; 50                          ; 0xc3844
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3845
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3848
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc384b
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc384e
    call 03de5h                               ; e8 91 05                    ; 0xc3851
    jmp short 038a3h                          ; eb 4d                       ; 0xc3854 vgabios.c:2612
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3856 vgabios.c:2614
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3859
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc385c
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc385f
    call 03e71h                               ; e8 0c 06                    ; 0xc3862
    jmp short 038a3h                          ; eb 3c                       ; 0xc3865 vgabios.c:2615
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3867 vgabios.c:2637
    jmp short 038a3h                          ; eb 35                       ; 0xc386c vgabios.c:2640
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc386e vgabios.c:2642
    jmp short 038a3h                          ; eb 2e                       ; 0xc3873 vgabios.c:2644
    call 007bfh                               ; e8 47 cf                    ; 0xc3875 vgabios.c:2646
    test ax, ax                               ; 85 c0                       ; 0xc3878
    je short 0389eh                           ; 74 22                       ; 0xc387a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc387c vgabios.c:2647
    xor ah, ah                                ; 30 e4                       ; 0xc387f
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3881
    jne short 03897h                          ; 75 11                       ; 0xc3884
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3886 vgabios.c:2650
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3889
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc388c
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc388f
    call 03f40h                               ; e8 ab 06                    ; 0xc3892
    jmp short 038a3h                          ; eb 0c                       ; 0xc3895 vgabios.c:2651
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3897 vgabios.c:2653
    jmp short 038a3h                          ; eb 05                       ; 0xc389c vgabios.c:2656
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc389e vgabios.c:2658
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc38a3 vgabios.c:2668
    pop di                                    ; 5f                          ; 0xc38a6
    pop si                                    ; 5e                          ; 0xc38a7
    pop bp                                    ; 5d                          ; 0xc38a8
    retn                                      ; c3                          ; 0xc38a9
  ; disGetNextSymbol 0xc38aa LB 0x77b -> off=0x0 cb=000000000000001f uValue=00000000000c38aa 'dispi_set_xres'
dispi_set_xres:                              ; 0xc38aa LB 0x1f
    push bp                                   ; 55                          ; 0xc38aa vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc38ab
    push bx                                   ; 53                          ; 0xc38ad
    push dx                                   ; 52                          ; 0xc38ae
    mov bx, ax                                ; 89 c3                       ; 0xc38af
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc38b1 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38b4
    call 00570h                               ; e8 b6 cc                    ; 0xc38b7
    mov ax, bx                                ; 89 d8                       ; 0xc38ba vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38bc
    call 00570h                               ; e8 ae cc                    ; 0xc38bf
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc38c2 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc38c5
    pop bx                                    ; 5b                          ; 0xc38c6
    pop bp                                    ; 5d                          ; 0xc38c7
    retn                                      ; c3                          ; 0xc38c8
  ; disGetNextSymbol 0xc38c9 LB 0x75c -> off=0x0 cb=000000000000001f uValue=00000000000c38c9 'dispi_set_yres'
dispi_set_yres:                              ; 0xc38c9 LB 0x1f
    push bp                                   ; 55                          ; 0xc38c9 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc38ca
    push bx                                   ; 53                          ; 0xc38cc
    push dx                                   ; 52                          ; 0xc38cd
    mov bx, ax                                ; 89 c3                       ; 0xc38ce
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc38d0 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38d3
    call 00570h                               ; e8 97 cc                    ; 0xc38d6
    mov ax, bx                                ; 89 d8                       ; 0xc38d9 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38db
    call 00570h                               ; e8 8f cc                    ; 0xc38de
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc38e1 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc38e4
    pop bx                                    ; 5b                          ; 0xc38e5
    pop bp                                    ; 5d                          ; 0xc38e6
    retn                                      ; c3                          ; 0xc38e7
  ; disGetNextSymbol 0xc38e8 LB 0x73d -> off=0x0 cb=0000000000000019 uValue=00000000000c38e8 'dispi_get_yres'
dispi_get_yres:                              ; 0xc38e8 LB 0x19
    push bp                                   ; 55                          ; 0xc38e8 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc38e9
    push dx                                   ; 52                          ; 0xc38eb
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc38ec vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38ef
    call 00570h                               ; e8 7b cc                    ; 0xc38f2
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38f5 vbe.c:121
    call 00577h                               ; e8 7c cc                    ; 0xc38f8
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc38fb vbe.c:122
    pop dx                                    ; 5a                          ; 0xc38fe
    pop bp                                    ; 5d                          ; 0xc38ff
    retn                                      ; c3                          ; 0xc3900
  ; disGetNextSymbol 0xc3901 LB 0x724 -> off=0x0 cb=000000000000001f uValue=00000000000c3901 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3901 LB 0x1f
    push bp                                   ; 55                          ; 0xc3901 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3902
    push bx                                   ; 53                          ; 0xc3904
    push dx                                   ; 52                          ; 0xc3905
    mov bx, ax                                ; 89 c3                       ; 0xc3906
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3908 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc390b
    call 00570h                               ; e8 5f cc                    ; 0xc390e
    mov ax, bx                                ; 89 d8                       ; 0xc3911 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3913
    call 00570h                               ; e8 57 cc                    ; 0xc3916
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3919 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc391c
    pop bx                                    ; 5b                          ; 0xc391d
    pop bp                                    ; 5d                          ; 0xc391e
    retn                                      ; c3                          ; 0xc391f
  ; disGetNextSymbol 0xc3920 LB 0x705 -> off=0x0 cb=0000000000000019 uValue=00000000000c3920 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3920 LB 0x19
    push bp                                   ; 55                          ; 0xc3920 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3921
    push dx                                   ; 52                          ; 0xc3923
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3924 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3927
    call 00570h                               ; e8 43 cc                    ; 0xc392a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc392d vbe.c:136
    call 00577h                               ; e8 44 cc                    ; 0xc3930
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3933 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3936
    pop bp                                    ; 5d                          ; 0xc3937
    retn                                      ; c3                          ; 0xc3938
  ; disGetNextSymbol 0xc3939 LB 0x6ec -> off=0x0 cb=000000000000001f uValue=00000000000c3939 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3939 LB 0x1f
    push bp                                   ; 55                          ; 0xc3939 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc393a
    push bx                                   ; 53                          ; 0xc393c
    push dx                                   ; 52                          ; 0xc393d
    mov bx, ax                                ; 89 c3                       ; 0xc393e
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3940 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3943
    call 00570h                               ; e8 27 cc                    ; 0xc3946
    mov ax, bx                                ; 89 d8                       ; 0xc3949 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc394b
    call 00570h                               ; e8 1f cc                    ; 0xc394e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3951 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3954
    pop bx                                    ; 5b                          ; 0xc3955
    pop bp                                    ; 5d                          ; 0xc3956
    retn                                      ; c3                          ; 0xc3957
  ; disGetNextSymbol 0xc3958 LB 0x6cd -> off=0x0 cb=0000000000000019 uValue=00000000000c3958 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3958 LB 0x19
    push bp                                   ; 55                          ; 0xc3958 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3959
    push dx                                   ; 52                          ; 0xc395b
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc395c vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc395f
    call 00570h                               ; e8 0b cc                    ; 0xc3962
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3965 vbe.c:151
    call 00577h                               ; e8 0c cc                    ; 0xc3968
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc396b vbe.c:152
    pop dx                                    ; 5a                          ; 0xc396e
    pop bp                                    ; 5d                          ; 0xc396f
    retn                                      ; c3                          ; 0xc3970
  ; disGetNextSymbol 0xc3971 LB 0x6b4 -> off=0x0 cb=0000000000000019 uValue=00000000000c3971 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3971 LB 0x19
    push bp                                   ; 55                          ; 0xc3971 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3972
    push dx                                   ; 52                          ; 0xc3974
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3975 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3978
    call 00570h                               ; e8 f2 cb                    ; 0xc397b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc397e vbe.c:157
    call 00577h                               ; e8 f3 cb                    ; 0xc3981
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3984 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3987
    pop bp                                    ; 5d                          ; 0xc3988
    retn                                      ; c3                          ; 0xc3989
  ; disGetNextSymbol 0xc398a LB 0x69b -> off=0x0 cb=0000000000000012 uValue=00000000000c398a 'in_word'
in_word:                                     ; 0xc398a LB 0x12
    push bp                                   ; 55                          ; 0xc398a vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc398b
    push bx                                   ; 53                          ; 0xc398d
    mov bx, ax                                ; 89 c3                       ; 0xc398e
    mov ax, dx                                ; 89 d0                       ; 0xc3990
    mov dx, bx                                ; 89 da                       ; 0xc3992 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3994
    in ax, DX                                 ; ed                          ; 0xc3995 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3996 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3999
    pop bp                                    ; 5d                          ; 0xc399a
    retn                                      ; c3                          ; 0xc399b
  ; disGetNextSymbol 0xc399c LB 0x689 -> off=0x0 cb=0000000000000014 uValue=00000000000c399c 'in_byte'
in_byte:                                     ; 0xc399c LB 0x14
    push bp                                   ; 55                          ; 0xc399c vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc399d
    push bx                                   ; 53                          ; 0xc399f
    mov bx, ax                                ; 89 c3                       ; 0xc39a0
    mov ax, dx                                ; 89 d0                       ; 0xc39a2
    mov dx, bx                                ; 89 da                       ; 0xc39a4 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc39a6
    in AL, DX                                 ; ec                          ; 0xc39a7 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc39a8
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc39aa vbe.c:170
    pop bx                                    ; 5b                          ; 0xc39ad
    pop bp                                    ; 5d                          ; 0xc39ae
    retn                                      ; c3                          ; 0xc39af
  ; disGetNextSymbol 0xc39b0 LB 0x675 -> off=0x0 cb=0000000000000014 uValue=00000000000c39b0 'dispi_get_id'
dispi_get_id:                                ; 0xc39b0 LB 0x14
    push bp                                   ; 55                          ; 0xc39b0 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc39b1
    push dx                                   ; 52                          ; 0xc39b3
    xor ax, ax                                ; 31 c0                       ; 0xc39b4 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc39b6
    out DX, ax                                ; ef                          ; 0xc39b9
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc39ba vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc39bd
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc39be vbe.c:177
    pop dx                                    ; 5a                          ; 0xc39c1
    pop bp                                    ; 5d                          ; 0xc39c2
    retn                                      ; c3                          ; 0xc39c3
  ; disGetNextSymbol 0xc39c4 LB 0x661 -> off=0x0 cb=000000000000001a uValue=00000000000c39c4 'dispi_set_id'
dispi_set_id:                                ; 0xc39c4 LB 0x1a
    push bp                                   ; 55                          ; 0xc39c4 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc39c5
    push bx                                   ; 53                          ; 0xc39c7
    push dx                                   ; 52                          ; 0xc39c8
    mov bx, ax                                ; 89 c3                       ; 0xc39c9
    xor ax, ax                                ; 31 c0                       ; 0xc39cb vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc39cd
    out DX, ax                                ; ef                          ; 0xc39d0
    mov ax, bx                                ; 89 d8                       ; 0xc39d1 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc39d3
    out DX, ax                                ; ef                          ; 0xc39d6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc39d7 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc39da
    pop bx                                    ; 5b                          ; 0xc39db
    pop bp                                    ; 5d                          ; 0xc39dc
    retn                                      ; c3                          ; 0xc39dd
  ; disGetNextSymbol 0xc39de LB 0x647 -> off=0x0 cb=000000000000002a uValue=00000000000c39de 'vbe_init'
vbe_init:                                    ; 0xc39de LB 0x2a
    push bp                                   ; 55                          ; 0xc39de vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc39df
    push bx                                   ; 53                          ; 0xc39e1
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc39e2 vbe.c:190
    call 039c4h                               ; e8 dc ff                    ; 0xc39e5
    call 039b0h                               ; e8 c5 ff                    ; 0xc39e8 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc39eb
    jne short 03a02h                          ; 75 12                       ; 0xc39ee
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc39f0 vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc39f3
    mov es, ax                                ; 8e c0                       ; 0xc39f6
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc39f8
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc39fc vbe.c:194
    call 039c4h                               ; e8 c2 ff                    ; 0xc39ff
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3a02 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3a05
    pop bp                                    ; 5d                          ; 0xc3a06
    retn                                      ; c3                          ; 0xc3a07
  ; disGetNextSymbol 0xc3a08 LB 0x61d -> off=0x0 cb=0000000000000055 uValue=00000000000c3a08 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3a08 LB 0x55
    push bp                                   ; 55                          ; 0xc3a08 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3a09
    push bx                                   ; 53                          ; 0xc3a0b
    push cx                                   ; 51                          ; 0xc3a0c
    push si                                   ; 56                          ; 0xc3a0d
    push di                                   ; 57                          ; 0xc3a0e
    mov di, ax                                ; 89 c7                       ; 0xc3a0f
    mov si, dx                                ; 89 d6                       ; 0xc3a11
    xor dx, dx                                ; 31 d2                       ; 0xc3a13 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a15
    call 0398ah                               ; e8 6f ff                    ; 0xc3a18
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3a1b vbe.c:209
    jne short 03a52h                          ; 75 32                       ; 0xc3a1e
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3a20 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3a23 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a25
    call 0398ah                               ; e8 5f ff                    ; 0xc3a28
    mov cx, ax                                ; 89 c1                       ; 0xc3a2b
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3a2d vbe.c:219
    je short 03a52h                           ; 74 20                       ; 0xc3a30
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3a32 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a35
    call 0398ah                               ; e8 4f ff                    ; 0xc3a38
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3a3b
    cmp cx, di                                ; 39 f9                       ; 0xc3a3e vbe.c:223
    jne short 03a4eh                          ; 75 0c                       ; 0xc3a40
    test si, si                               ; 85 f6                       ; 0xc3a42 vbe.c:225
    jne short 03a4ah                          ; 75 04                       ; 0xc3a44
    mov ax, bx                                ; 89 d8                       ; 0xc3a46 vbe.c:226
    jmp short 03a54h                          ; eb 0a                       ; 0xc3a48
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3a4a vbe.c:227
    jne short 03a46h                          ; 75 f8                       ; 0xc3a4c
    mov bx, dx                                ; 89 d3                       ; 0xc3a4e vbe.c:230
    jmp short 03a25h                          ; eb d3                       ; 0xc3a50 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3a52 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3a54 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3a57
    pop si                                    ; 5e                          ; 0xc3a58
    pop cx                                    ; 59                          ; 0xc3a59
    pop bx                                    ; 5b                          ; 0xc3a5a
    pop bp                                    ; 5d                          ; 0xc3a5b
    retn                                      ; c3                          ; 0xc3a5c
  ; disGetNextSymbol 0xc3a5d LB 0x5c8 -> off=0x0 cb=0000000000000125 uValue=00000000000c3a5d 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3a5d LB 0x125
    push bp                                   ; 55                          ; 0xc3a5d vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3a5e
    push cx                                   ; 51                          ; 0xc3a60
    push si                                   ; 56                          ; 0xc3a61
    push di                                   ; 57                          ; 0xc3a62
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3a63
    mov si, ax                                ; 89 c6                       ; 0xc3a66
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3a68
    mov di, bx                                ; 89 df                       ; 0xc3a6b
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3a6d vbe.c:275
    call 005b7h                               ; e8 42 cb                    ; 0xc3a72 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3a75
    mov bx, di                                ; 89 fb                       ; 0xc3a78 vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3a7a
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3a7d
    xor dx, dx                                ; 31 d2                       ; 0xc3a80 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a82
    call 0398ah                               ; e8 02 ff                    ; 0xc3a85
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3a88 vbe.c:285
    je short 03a97h                           ; 74 0a                       ; 0xc3a8b
    push SS                                   ; 16                          ; 0xc3a8d vbe.c:287
    pop ES                                    ; 07                          ; 0xc3a8e
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3a8f
    jmp near 03b7ah                           ; e9 e3 00                    ; 0xc3a94 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3a97 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3a9a vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3a9f vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3aa2
    jne short 03ab1h                          ; 75 07                       ; 0xc3aa8
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3aaa
    je short 03ac0h                           ; 74 0f                       ; 0xc3aaf
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3ab1
    jne short 03ac5h                          ; 75 0c                       ; 0xc3ab7
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3ab9
    jne short 03ac5h                          ; 75 05                       ; 0xc3abe
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3ac0 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3ac5 vbe.c:318
    db  066h, 026h, 0c7h, 007h, 056h, 045h, 053h, 041h
    ; mov dword [es:bx], strict dword 041534556h ; 66 26 c7 07 56 45 53 41  ; 0xc3ac8
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3ad0 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3ad6 vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3adc
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 000h
    ; mov dword [es:bx+00ah], strict dword 000000001h ; 66 26 c7 47 0a 01 00 00 00; 0xc3ae0 vbe.c:330
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ae9 vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3aec
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3af0 vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3af3
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3af7 vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3afa
    call 0398ah                               ; e8 8a fe                    ; 0xc3afd
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3b00
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3b03
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3b07 vbe.c:342
    je short 03b31h                           ; 74 24                       ; 0xc3b0b
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3b0d vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3b13 vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3b19
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3b1d vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3b23
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3b27 vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3b2d
    mov dx, cx                                ; 89 ca                       ; 0xc3b31 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3b33
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b36
    call 0399ch                               ; e8 60 fe                    ; 0xc3b39
    xor ah, ah                                ; 30 e4                       ; 0xc3b3c vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3b3e
    jnbe short 03b5ah                         ; 77 17                       ; 0xc3b41
    mov dx, cx                                ; 89 ca                       ; 0xc3b43 vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b45
    call 0398ah                               ; e8 3f fe                    ; 0xc3b48
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3b4b vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3b4e
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3b50 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3b53
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3b56 vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3b5a vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3b5d vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b5f
    call 0398ah                               ; e8 25 fe                    ; 0xc3b62
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3b65 vbe.c:368
    jne short 03b31h                          ; 75 c7                       ; 0xc3b68
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3b6a vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3b6d vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3b70
    push SS                                   ; 16                          ; 0xc3b73 vbe.c:372
    pop ES                                    ; 07                          ; 0xc3b74
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3b75
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3b7a vbe.c:373
    pop di                                    ; 5f                          ; 0xc3b7d
    pop si                                    ; 5e                          ; 0xc3b7e
    pop cx                                    ; 59                          ; 0xc3b7f
    pop bp                                    ; 5d                          ; 0xc3b80
    retn                                      ; c3                          ; 0xc3b81
  ; disGetNextSymbol 0xc3b82 LB 0x4a3 -> off=0x0 cb=000000000000009c uValue=00000000000c3b82 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3b82 LB 0x9c
    push bp                                   ; 55                          ; 0xc3b82 vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3b83
    push si                                   ; 56                          ; 0xc3b85
    push di                                   ; 57                          ; 0xc3b86
    push ax                                   ; 50                          ; 0xc3b87
    push ax                                   ; 50                          ; 0xc3b88
    mov ax, dx                                ; 89 d0                       ; 0xc3b89
    mov si, bx                                ; 89 de                       ; 0xc3b8b
    mov bx, cx                                ; 89 cb                       ; 0xc3b8d
    test dh, 040h                             ; f6 c6 40                    ; 0xc3b8f vbe.c:396
    db  00fh, 095h, 0c2h
    ; setne dl                                  ; 0f 95 c2                  ; 0xc3b92
    xor dh, dh                                ; 30 f6                       ; 0xc3b95
    and ah, 001h                              ; 80 e4 01                    ; 0xc3b97 vbe.c:397
    call 03a08h                               ; e8 6b fe                    ; 0xc3b9a vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3b9d
    test ax, ax                               ; 85 c0                       ; 0xc3ba0 vbe.c:401
    je short 03c0ch                           ; 74 68                       ; 0xc3ba2
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3ba4 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3ba7
    mov di, bx                                ; 89 df                       ; 0xc3ba9
    mov es, si                                ; 8e c6                       ; 0xc3bab
    cld                                       ; fc                          ; 0xc3bad
    jcxz 03bb2h                               ; e3 02                       ; 0xc3bae
    rep stosb                                 ; f3 aa                       ; 0xc3bb0
    xor cx, cx                                ; 31 c9                       ; 0xc3bb2 vbe.c:407
    jmp short 03bbbh                          ; eb 05                       ; 0xc3bb4
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3bb6
    jnc short 03bd4h                          ; 73 19                       ; 0xc3bb9
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3bbb vbe.c:410
    inc dx                                    ; 42                          ; 0xc3bbe
    inc dx                                    ; 42                          ; 0xc3bbf
    add dx, cx                                ; 01 ca                       ; 0xc3bc0
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3bc2
    call 0399ch                               ; e8 d4 fd                    ; 0xc3bc5
    mov di, bx                                ; 89 df                       ; 0xc3bc8 vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3bca
    mov es, si                                ; 8e c6                       ; 0xc3bcc vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3bce
    inc cx                                    ; 41                          ; 0xc3bd1 vbe.c:412
    jmp short 03bb6h                          ; eb e2                       ; 0xc3bd2
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3bd4 vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3bd7 vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3bd9
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3bdc vbe.c:414
    je short 03bf0h                           ; 74 10                       ; 0xc3bde
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3be0 vbe.c:415
    mov word [es:di], 00629h                  ; 26 c7 05 29 06              ; 0xc3be3 vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3be8 vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3beb vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3bf0 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bf3
    call 00570h                               ; e8 77 c9                    ; 0xc3bf6
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bf9 vbe.c:421
    call 00577h                               ; e8 78 c9                    ; 0xc3bfc
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3bff
    mov es, si                                ; 8e c6                       ; 0xc3c02 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3c04
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3c07 vbe.c:423
    jmp short 03c0fh                          ; eb 03                       ; 0xc3c0a vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3c0c vbe.c:428
    push SS                                   ; 16                          ; 0xc3c0f vbe.c:431
    pop ES                                    ; 07                          ; 0xc3c10
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3c11
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3c14
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c17 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3c1a
    pop si                                    ; 5e                          ; 0xc3c1b
    pop bp                                    ; 5d                          ; 0xc3c1c
    retn                                      ; c3                          ; 0xc3c1d
  ; disGetNextSymbol 0xc3c1e LB 0x407 -> off=0x0 cb=00000000000000e5 uValue=00000000000c3c1e 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3c1e LB 0xe5
    push bp                                   ; 55                          ; 0xc3c1e vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3c1f
    push si                                   ; 56                          ; 0xc3c21
    push di                                   ; 57                          ; 0xc3c22
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3c23
    mov si, ax                                ; 89 c6                       ; 0xc3c26
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3c28
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3c2b vbe.c:452
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0                  ; 0xc3c2f
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3c32
    mov ax, dx                                ; 89 d0                       ; 0xc3c35
    test dx, dx                               ; 85 d2                       ; 0xc3c37 vbe.c:453
    je short 03c3eh                           ; 74 03                       ; 0xc3c39
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3c3b
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc3c3e
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3c41 vbe.c:454
    je short 03c4ch                           ; 74 05                       ; 0xc3c45
    mov dx, 00080h                            ; ba 80 00                    ; 0xc3c47
    jmp short 03c4eh                          ; eb 02                       ; 0xc3c4a
    xor dx, dx                                ; 31 d2                       ; 0xc3c4c
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc3c4e
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3c51 vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3c55 vbe.c:459
    jnc short 03c6eh                          ; 73 12                       ; 0xc3c5a
    xor ax, ax                                ; 31 c0                       ; 0xc3c5c vbe.c:463
    call 005ddh                               ; e8 7c c9                    ; 0xc3c5e
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc3c61 vbe.c:467
    call 0130eh                               ; e8 a6 d6                    ; 0xc3c65
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3c68 vbe.c:468
    jmp near 03cf7h                           ; e9 89 00                    ; 0xc3c6b vbe.c:469
    mov dx, ax                                ; 89 c2                       ; 0xc3c6e vbe.c:472
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3c70
    call 03a08h                               ; e8 92 fd                    ; 0xc3c73
    mov bx, ax                                ; 89 c3                       ; 0xc3c76
    test ax, ax                               ; 85 c0                       ; 0xc3c78 vbe.c:474
    je short 03cf4h                           ; 74 78                       ; 0xc3c7a
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3c7c vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c7f
    call 0398ah                               ; e8 05 fd                    ; 0xc3c82
    mov cx, ax                                ; 89 c1                       ; 0xc3c85
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3c87 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c8a
    call 0398ah                               ; e8 fa fc                    ; 0xc3c8d
    mov di, ax                                ; 89 c7                       ; 0xc3c90
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3c92 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c95
    call 0399ch                               ; e8 01 fd                    ; 0xc3c98
    mov bl, al                                ; 88 c3                       ; 0xc3c9b
    mov dl, al                                ; 88 c2                       ; 0xc3c9d
    xor ax, ax                                ; 31 c0                       ; 0xc3c9f vbe.c:489
    call 005ddh                               ; e8 39 c9                    ; 0xc3ca1
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3ca4 vbe.c:491
    jne short 03cafh                          ; 75 06                       ; 0xc3ca7
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3ca9 vbe.c:493
    call 0130eh                               ; e8 5f d6                    ; 0xc3cac
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc3caf vbe.c:496
    call 03901h                               ; e8 4c fc                    ; 0xc3cb2
    mov ax, cx                                ; 89 c8                       ; 0xc3cb5 vbe.c:497
    call 038aah                               ; e8 f0 fb                    ; 0xc3cb7
    mov ax, di                                ; 89 f8                       ; 0xc3cba vbe.c:498
    call 038c9h                               ; e8 0a fc                    ; 0xc3cbc
    xor ax, ax                                ; 31 c0                       ; 0xc3cbf vbe.c:499
    call 00603h                               ; e8 3f c9                    ; 0xc3cc1
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3cc4 vbe.c:500
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3cc7
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3cc9
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3ccc
    or ax, dx                                 ; 09 d0                       ; 0xc3cd0
    call 005ddh                               ; e8 08 c9                    ; 0xc3cd2
    call 006d2h                               ; e8 fa c9                    ; 0xc3cd5 vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3cd8 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3cdb
    mov es, ax                                ; 8e c0                       ; 0xc3cde
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ce0
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3ce3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3ce6 vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3ce9
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3ceb vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3cee
    jmp near 03c68h                           ; e9 74 ff                    ; 0xc3cf1
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3cf4 vbe.c:513
    push SS                                   ; 16                          ; 0xc3cf7 vbe.c:517
    pop ES                                    ; 07                          ; 0xc3cf8
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3cf9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3cfc vbe.c:518
    pop di                                    ; 5f                          ; 0xc3cff
    pop si                                    ; 5e                          ; 0xc3d00
    pop bp                                    ; 5d                          ; 0xc3d01
    retn                                      ; c3                          ; 0xc3d02
  ; disGetNextSymbol 0xc3d03 LB 0x322 -> off=0x0 cb=0000000000000008 uValue=00000000000c3d03 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3d03 LB 0x8
    push bp                                   ; 55                          ; 0xc3d03 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3d04
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3d06 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3d09
    retn                                      ; c3                          ; 0xc3d0a
  ; disGetNextSymbol 0xc3d0b LB 0x31a -> off=0x0 cb=000000000000004b uValue=00000000000c3d0b 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3d0b LB 0x4b
    push bp                                   ; 55                          ; 0xc3d0b vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3d0c
    push bx                                   ; 53                          ; 0xc3d0e
    push cx                                   ; 51                          ; 0xc3d0f
    push si                                   ; 56                          ; 0xc3d10
    mov si, ax                                ; 89 c6                       ; 0xc3d11
    mov bx, dx                                ; 89 d3                       ; 0xc3d13
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3d15 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d18
    out DX, ax                                ; ef                          ; 0xc3d1b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d1c vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3d1f
    mov es, si                                ; 8e c6                       ; 0xc3d20 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d22
    inc bx                                    ; 43                          ; 0xc3d25 vbe.c:532
    inc bx                                    ; 43                          ; 0xc3d26
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3d27 vbe.c:533
    je short 03d4eh                           ; 74 23                       ; 0xc3d29
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3d2b vbe.c:535
    jmp short 03d35h                          ; eb 05                       ; 0xc3d2e
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3d30
    jnbe short 03d4eh                         ; 77 19                       ; 0xc3d33
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3d35 vbe.c:536
    je short 03d4bh                           ; 74 11                       ; 0xc3d38
    mov ax, cx                                ; 89 c8                       ; 0xc3d3a vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d3c
    out DX, ax                                ; ef                          ; 0xc3d3f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d40 vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3d43
    mov es, si                                ; 8e c6                       ; 0xc3d44 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d46
    inc bx                                    ; 43                          ; 0xc3d49 vbe.c:539
    inc bx                                    ; 43                          ; 0xc3d4a
    inc cx                                    ; 41                          ; 0xc3d4b vbe.c:541
    jmp short 03d30h                          ; eb e2                       ; 0xc3d4c
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3d4e vbe.c:542
    pop si                                    ; 5e                          ; 0xc3d51
    pop cx                                    ; 59                          ; 0xc3d52
    pop bx                                    ; 5b                          ; 0xc3d53
    pop bp                                    ; 5d                          ; 0xc3d54
    retn                                      ; c3                          ; 0xc3d55
  ; disGetNextSymbol 0xc3d56 LB 0x2cf -> off=0x0 cb=000000000000008f uValue=00000000000c3d56 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3d56 LB 0x8f
    push bp                                   ; 55                          ; 0xc3d56 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3d57
    push bx                                   ; 53                          ; 0xc3d59
    push cx                                   ; 51                          ; 0xc3d5a
    push si                                   ; 56                          ; 0xc3d5b
    push ax                                   ; 50                          ; 0xc3d5c
    mov cx, ax                                ; 89 c1                       ; 0xc3d5d
    mov bx, dx                                ; 89 d3                       ; 0xc3d5f
    mov es, ax                                ; 8e c0                       ; 0xc3d61 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d63
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3d66
    inc bx                                    ; 43                          ; 0xc3d69 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3d6a
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3d6b vbe.c:552
    jne short 03d81h                          ; 75 10                       ; 0xc3d6f
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3d71 vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d74
    out DX, ax                                ; ef                          ; 0xc3d77
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3d78 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d7b
    out DX, ax                                ; ef                          ; 0xc3d7e
    jmp short 03dddh                          ; eb 5c                       ; 0xc3d7f vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3d81 vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d84
    out DX, ax                                ; ef                          ; 0xc3d87
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d88 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d8b vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3d8e
    inc bx                                    ; 43                          ; 0xc3d8f vbe.c:558
    inc bx                                    ; 43                          ; 0xc3d90
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3d91
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d94
    out DX, ax                                ; ef                          ; 0xc3d97
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d98 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d9b vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3d9e
    inc bx                                    ; 43                          ; 0xc3d9f vbe.c:561
    inc bx                                    ; 43                          ; 0xc3da0
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3da1
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3da4
    out DX, ax                                ; ef                          ; 0xc3da7
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3da8 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3dab vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3dae
    inc bx                                    ; 43                          ; 0xc3daf vbe.c:564
    inc bx                                    ; 43                          ; 0xc3db0
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3db1
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3db4
    out DX, ax                                ; ef                          ; 0xc3db7
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3db8 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3dbb
    out DX, ax                                ; ef                          ; 0xc3dbe
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3dbf vbe.c:568
    jmp short 03dc9h                          ; eb 05                       ; 0xc3dc2
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3dc4
    jnbe short 03dddh                         ; 77 14                       ; 0xc3dc7
    mov ax, si                                ; 89 f0                       ; 0xc3dc9 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3dcb
    out DX, ax                                ; ef                          ; 0xc3dce
    mov es, cx                                ; 8e c1                       ; 0xc3dcf vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3dd1
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3dd4 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3dd7
    inc bx                                    ; 43                          ; 0xc3dd8 vbe.c:571
    inc bx                                    ; 43                          ; 0xc3dd9
    inc si                                    ; 46                          ; 0xc3dda vbe.c:572
    jmp short 03dc4h                          ; eb e7                       ; 0xc3ddb
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3ddd vbe.c:574
    pop si                                    ; 5e                          ; 0xc3de0
    pop cx                                    ; 59                          ; 0xc3de1
    pop bx                                    ; 5b                          ; 0xc3de2
    pop bp                                    ; 5d                          ; 0xc3de3
    retn                                      ; c3                          ; 0xc3de4
  ; disGetNextSymbol 0xc3de5 LB 0x240 -> off=0x0 cb=000000000000008c uValue=00000000000c3de5 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc3de5 LB 0x8c
    push bp                                   ; 55                          ; 0xc3de5 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc3de6
    push si                                   ; 56                          ; 0xc3de8
    push di                                   ; 57                          ; 0xc3de9
    push ax                                   ; 50                          ; 0xc3dea
    mov si, ax                                ; 89 c6                       ; 0xc3deb
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc3ded
    mov ax, bx                                ; 89 d8                       ; 0xc3df0
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc3df2
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3df5 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc3df8 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3dfa
    je short 03e44h                           ; 74 45                       ; 0xc3dfd
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3dff
    je short 03e28h                           ; 74 24                       ; 0xc3e02
    test ax, ax                               ; 85 c0                       ; 0xc3e04
    jne short 03e60h                          ; 75 58                       ; 0xc3e06
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3e08 vbe.c:598
    call 02df7h                               ; e8 e9 ef                    ; 0xc3e0b
    mov cx, ax                                ; 89 c1                       ; 0xc3e0e
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3e10 vbe.c:602
    je short 03e1bh                           ; 74 05                       ; 0xc3e14
    call 03d03h                               ; e8 ea fe                    ; 0xc3e16 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc3e19
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3e1b vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3e1e
    push SS                                   ; 16                          ; 0xc3e21
    pop ES                                    ; 07                          ; 0xc3e22
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e23
    jmp short 03e63h                          ; eb 3b                       ; 0xc3e26 vbe.c:605
    push SS                                   ; 16                          ; 0xc3e28 vbe.c:607
    pop ES                                    ; 07                          ; 0xc3e29
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3e2a
    mov dx, cx                                ; 89 ca                       ; 0xc3e2d vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3e2f
    call 02e32h                               ; e8 fd ef                    ; 0xc3e32
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3e35 vbe.c:612
    je short 03e63h                           ; 74 28                       ; 0xc3e39
    mov dx, ax                                ; 89 c2                       ; 0xc3e3b vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc3e3d
    call 03d0bh                               ; e8 c9 fe                    ; 0xc3e3f
    jmp short 03e63h                          ; eb 1f                       ; 0xc3e42 vbe.c:614
    push SS                                   ; 16                          ; 0xc3e44 vbe.c:616
    pop ES                                    ; 07                          ; 0xc3e45
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3e46
    mov dx, cx                                ; 89 ca                       ; 0xc3e49 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3e4b
    call 03108h                               ; e8 b7 f2                    ; 0xc3e4e
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3e51 vbe.c:621
    je short 03e63h                           ; 74 0c                       ; 0xc3e55
    mov dx, ax                                ; 89 c2                       ; 0xc3e57 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc3e59
    call 03d56h                               ; e8 f8 fe                    ; 0xc3e5b
    jmp short 03e63h                          ; eb 03                       ; 0xc3e5e vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc3e60 vbe.c:626
    push SS                                   ; 16                          ; 0xc3e63 vbe.c:629
    pop ES                                    ; 07                          ; 0xc3e64
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc3e65
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e68 vbe.c:630
    pop di                                    ; 5f                          ; 0xc3e6b
    pop si                                    ; 5e                          ; 0xc3e6c
    pop bp                                    ; 5d                          ; 0xc3e6d
    retn 00002h                               ; c2 02 00                    ; 0xc3e6e
  ; disGetNextSymbol 0xc3e71 LB 0x1b4 -> off=0x0 cb=00000000000000cf uValue=00000000000c3e71 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc3e71 LB 0xcf
    push bp                                   ; 55                          ; 0xc3e71 vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc3e72
    push si                                   ; 56                          ; 0xc3e74
    push di                                   ; 57                          ; 0xc3e75
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc3e76
    push ax                                   ; 50                          ; 0xc3e79
    mov di, dx                                ; 89 d7                       ; 0xc3e7a
    mov si, bx                                ; 89 de                       ; 0xc3e7c
    mov word [bp-008h], cx                    ; 89 4e f8                    ; 0xc3e7e
    call 03920h                               ; e8 9c fa                    ; 0xc3e81 vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3e84 vbe.c:661
    jne short 03e8dh                          ; 75 05                       ; 0xc3e86
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3e88
    jmp short 03e90h                          ; eb 03                       ; 0xc3e8b
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3e8d
    call 03958h                               ; e8 c5 fa                    ; 0xc3e90 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3e93
    mov word [bp-006h], strict word 0004fh    ; c7 46 fa 4f 00              ; 0xc3e96 vbe.c:663
    push SS                                   ; 16                          ; 0xc3e9b vbe.c:664
    pop ES                                    ; 07                          ; 0xc3e9c
    mov bx, word [es:si]                      ; 26 8b 1c                    ; 0xc3e9d
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3ea0 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc3ea3 vbe.c:669
    je short 03eb2h                           ; 74 0b                       ; 0xc3ea5
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc3ea7
    je short 03ed9h                           ; 74 2e                       ; 0xc3ea9
    test al, al                               ; 84 c0                       ; 0xc3eab
    je short 03ed4h                           ; 74 25                       ; 0xc3ead
    jmp near 03f29h                           ; e9 77 00                    ; 0xc3eaf
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3eb2 vbe.c:671
    jne short 03ebch                          ; 75 05                       ; 0xc3eb5
    sal bx, 003h                              ; c1 e3 03                    ; 0xc3eb7 vbe.c:672
    jmp short 03ed4h                          ; eb 18                       ; 0xc3eba vbe.c:673
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3ebc vbe.c:674
    cwd                                       ; 99                          ; 0xc3ebf
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3ec0
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3ec3
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3ec5
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3ec8
    mov ax, bx                                ; 89 d8                       ; 0xc3ecb
    xor dx, dx                                ; 31 d2                       ; 0xc3ecd
    div word [bp-00ch]                        ; f7 76 f4                    ; 0xc3ecf
    mov bx, ax                                ; 89 c3                       ; 0xc3ed2
    mov ax, bx                                ; 89 d8                       ; 0xc3ed4 vbe.c:677
    call 03939h                               ; e8 60 fa                    ; 0xc3ed6
    call 03958h                               ; e8 7c fa                    ; 0xc3ed9 vbe.c:680
    mov bx, ax                                ; 89 c3                       ; 0xc3edc
    push SS                                   ; 16                          ; 0xc3ede vbe.c:681
    pop ES                                    ; 07                          ; 0xc3edf
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3ee0
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3ee3 vbe.c:682
    jne short 03eedh                          ; 75 05                       ; 0xc3ee6
    shr bx, 003h                              ; c1 eb 03                    ; 0xc3ee8 vbe.c:683
    jmp short 03efch                          ; eb 0f                       ; 0xc3eeb vbe.c:684
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3eed vbe.c:685
    cwd                                       ; 99                          ; 0xc3ef0
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3ef1
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3ef4
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3ef6
    imul bx, ax                               ; 0f af d8                    ; 0xc3ef9
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc3efc vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc3eff
    push SS                                   ; 16                          ; 0xc3f02 vbe.c:687
    pop ES                                    ; 07                          ; 0xc3f03
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc3f04
    call 03971h                               ; e8 67 fa                    ; 0xc3f07 vbe.c:688
    push SS                                   ; 16                          ; 0xc3f0a
    pop ES                                    ; 07                          ; 0xc3f0b
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3f0c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f0f
    call 038e8h                               ; e8 d3 f9                    ; 0xc3f12 vbe.c:689
    push SS                                   ; 16                          ; 0xc3f15
    pop ES                                    ; 07                          ; 0xc3f16
    cmp ax, word [es:bx]                      ; 26 3b 07                    ; 0xc3f17
    jbe short 03f2eh                          ; 76 12                       ; 0xc3f1a
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3f1c vbe.c:690
    call 03939h                               ; e8 17 fa                    ; 0xc3f1f
    mov word [bp-006h], 00200h                ; c7 46 fa 00 02              ; 0xc3f22 vbe.c:691
    jmp short 03f2eh                          ; eb 05                       ; 0xc3f27 vbe.c:693
    mov word [bp-006h], 00100h                ; c7 46 fa 00 01              ; 0xc3f29 vbe.c:696
    push SS                                   ; 16                          ; 0xc3f2e vbe.c:699
    pop ES                                    ; 07                          ; 0xc3f2f
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3f30
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc3f33
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f36
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f39 vbe.c:700
    pop di                                    ; 5f                          ; 0xc3f3c
    pop si                                    ; 5e                          ; 0xc3f3d
    pop bp                                    ; 5d                          ; 0xc3f3e
    retn                                      ; c3                          ; 0xc3f3f
  ; disGetNextSymbol 0xc3f40 LB 0xe5 -> off=0x0 cb=00000000000000e5 uValue=00000000000c3f40 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc3f40 LB 0xe5
    push bp                                   ; 55                          ; 0xc3f40 vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc3f41
    push si                                   ; 56                          ; 0xc3f43
    push di                                   ; 57                          ; 0xc3f44
    push ax                                   ; 50                          ; 0xc3f45
    push ax                                   ; 50                          ; 0xc3f46
    push ax                                   ; 50                          ; 0xc3f47
    mov si, dx                                ; 89 d6                       ; 0xc3f48
    mov dx, cx                                ; 89 ca                       ; 0xc3f4a
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3f4c vbe.c:739
    push SS                                   ; 16                          ; 0xc3f4f vbe.c:740
    pop ES                                    ; 07                          ; 0xc3f50
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3f51
    test al, al                               ; 84 c0                       ; 0xc3f54 vbe.c:741
    jne short 03f7ah                          ; 75 22                       ; 0xc3f56
    push SS                                   ; 16                          ; 0xc3f58 vbe.c:743
    pop ES                                    ; 07                          ; 0xc3f59
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc3f5a
    mov bx, dx                                ; 89 d3                       ; 0xc3f5d vbe.c:744
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3f5f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3f62 vbe.c:745
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3f65
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3f68
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3f6b
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc3f6e vbe.c:750
    je short 03f80h                           ; 74 0e                       ; 0xc3f70
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc3f72
    je short 03f80h                           ; 74 0a                       ; 0xc3f74
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc3f76
    je short 03f80h                           ; 74 06                       ; 0xc3f78
    mov di, 00100h                            ; bf 00 01                    ; 0xc3f7a vbe.c:751
    jmp near 04016h                           ; e9 96 00                    ; 0xc3f7d vbe.c:752
    push SS                                   ; 16                          ; 0xc3f80 vbe.c:756
    pop ES                                    ; 07                          ; 0xc3f81
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc3f82
    je short 03f8eh                           ; 74 05                       ; 0xc3f87
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f89
    jmp short 03f90h                          ; eb 02                       ; 0xc3f8c
    xor ax, ax                                ; 31 c0                       ; 0xc3f8e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3f90
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc3f93 vbe.c:759
    jnc short 03f9eh                          ; 73 05                       ; 0xc3f97
    mov cx, 00280h                            ; b9 80 02                    ; 0xc3f99 vbe.c:760
    jmp short 03fa7h                          ; eb 09                       ; 0xc3f9c vbe.c:761
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc3f9e
    jbe short 03fa7h                          ; 76 03                       ; 0xc3fa2
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc3fa4 vbe.c:762
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc3fa7 vbe.c:763
    jnc short 03fb2h                          ; 73 05                       ; 0xc3fab
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc3fad vbe.c:764
    jmp short 03fbbh                          ; eb 09                       ; 0xc3fb0 vbe.c:765
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc3fb2
    jbe short 03fbbh                          ; 76 03                       ; 0xc3fb6
    mov bx, 00780h                            ; bb 80 07                    ; 0xc3fb8 vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3fbb vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fbe
    call 0398ah                               ; e8 c6 f9                    ; 0xc3fc1
    mov si, ax                                ; 89 c6                       ; 0xc3fc4
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3fc6 vbe.c:775
    cwd                                       ; 99                          ; 0xc3fca
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3fcb
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3fce
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3fd0
    imul ax, cx                               ; 0f af c1                    ; 0xc3fd3
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc3fd6 vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc3fd9
    mov dx, bx                                ; 89 da                       ; 0xc3fdb vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc3fdd
    cmp dx, si                                ; 39 f2                       ; 0xc3fdf vbe.c:780
    jnbe short 03fe9h                         ; 77 06                       ; 0xc3fe1
    jne short 03feeh                          ; 75 09                       ; 0xc3fe3
    test ax, ax                               ; 85 c0                       ; 0xc3fe5
    jbe short 03feeh                          ; 76 05                       ; 0xc3fe7
    mov di, 00200h                            ; bf 00 02                    ; 0xc3fe9 vbe.c:782
    jmp short 04016h                          ; eb 28                       ; 0xc3fec vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc3fee vbe.c:787
    call 005ddh                               ; e8 ea c5                    ; 0xc3ff0
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3ff3 vbe.c:788
    call 03901h                               ; e8 07 f9                    ; 0xc3ff7
    mov ax, cx                                ; 89 c8                       ; 0xc3ffa vbe.c:789
    call 038aah                               ; e8 ab f8                    ; 0xc3ffc
    mov ax, bx                                ; 89 d8                       ; 0xc3fff vbe.c:790
    call 038c9h                               ; e8 c5 f8                    ; 0xc4001
    xor ax, ax                                ; 31 c0                       ; 0xc4004 vbe.c:791
    call 00603h                               ; e8 fa c5                    ; 0xc4006
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4009 vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc400c
    xor ah, ah                                ; 30 e4                       ; 0xc400e
    call 005ddh                               ; e8 ca c5                    ; 0xc4010
    call 006d2h                               ; e8 bc c6                    ; 0xc4013 vbe.c:793
    push SS                                   ; 16                          ; 0xc4016 vbe.c:801
    pop ES                                    ; 07                          ; 0xc4017
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc4018
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc401b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc401e vbe.c:802
    pop di                                    ; 5f                          ; 0xc4021
    pop si                                    ; 5e                          ; 0xc4022
    pop bp                                    ; 5d                          ; 0xc4023
    retn                                      ; c3                          ; 0xc4024

  ; Padding 0x5db bytes at 0xc4025
  times 1499 db 0

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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 00ah
