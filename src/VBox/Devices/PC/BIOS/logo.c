#define COMPRESS_NONE  0
#define COMPRESS_RLE8  1
#define COMPRESS_RLE4  2

#define BMP_HEADER_OS21  12
#define BMP_HEADER_OS22  64
#define BMP_HEADER_WIN3  40

#define WAIT_HZ         64
#define WAIT_MS         16

#define F12_SCAN_CODE   0x86
#define F12_WAIT_TIME   (3 * WAIT_HZ)   /* 3 seconds. Used only if logo disabled. */

#define LOGO_IO_PORT    0x506

typedef struct
{
    Bit8u Blue;
    Bit8u Green;
    Bit8u Red;
} RGBPAL;

/* BMP File Format Bitmap Header. */
typedef struct
{
    Bit16u      Type;           /* File Type Identifier       */
    Bit32u      FileSize;       /* Size of File               */
    Bit16u      Reserved1;      /* Reserved (should be 0)     */
    Bit16u      Reserved2;      /* Reserved (should be 0)     */
    Bit32u      Offset;         /* Offset to bitmap data      */
} BMPINFO;

/* OS/2 1.x Information Header Format. */
typedef struct
{
    Bit32u      Size;           /* Size of Remianing Header   */
    Bit16u      Width;          /* Width of Bitmap in Pixels  */
    Bit16u      Height;         /* Height of Bitmap in Pixels */
    Bit16u      Planes;         /* Number of Planes           */
    Bit16u      BitCount;       /* Color Bits Per Pixel       */
} OS2HDR;

/* OS/2 2.0 Information Header Format. */
typedef struct
{
    Bit32u      Size;           /* Size of Remianing Header   */
    Bit32u      Width;          /* Width of Bitmap in Pixels        */
    Bit32u      Height;         /* Height of Bitmap in Pixels       */
    Bit16u      Planes;         /* Number of Planes                 */
    Bit16u      BitCount;       /* Color Bits Per Pixel             */
    Bit32u      Compression;    /* Compression Scheme (0=none)      */
    Bit32u      SizeImage;      /* Size of bitmap in bytes          */
    Bit32u      XPelsPerMeter;  /* Horz. Resolution in Pixels/Meter */
    Bit32u      YPelsPerMeter;  /* Vert. Resolution in Pixels/Meter */
    Bit32u      ClrUsed;        /* Number of Colors in Color Table  */
    Bit32u      ClrImportant;   /* Number of Important Colors       */
    Bit16u      Units;          /* Resolution Mesaurement Used      */
    Bit16u      Reserved;       /* Reserved FIelds (always 0)       */
    Bit16u      Recording;      /* Orientation of Bitmap            */
    Bit16u      Rendering;      /* Halftone Algorithm Used on Image */
    Bit32u      Size1;          /* Halftone Algorithm Data          */
    Bit32u      Size2;          /* Halftone Algorithm Data          */
    Bit32u      ColorEncoding;  /* Color Table Format (always 0)    */
    Bit32u      Identifier;     /* Misc. Field for Application Use  */
} OS22HDR;

/* Windows 3.x Information Header Format. */
typedef struct
{
    Bit32u      Size;           /* Size of Remianing Header   */
    Bit32u      Width;          /* Width of Bitmap in Pixels        */
    Bit32u      Height;         /* Height of Bitmap in Pixels       */
    Bit16u      Planes;         /* Number of Planes                 */
    Bit16u      BitCount;       /* Bits Per Pixel                   */
    Bit32u      Compression;    /* Compression Scheme (0=none)      */
    Bit32u      SizeImage;      /* Size of bitmap in bytes          */
    Bit32u      XPelsPerMeter;  /* Horz. Resolution in Pixels/Meter */
    Bit32u      YPelsPerMeter;  /* Vert. Resolution in Pixels/Meter */
    Bit32u      ClrUsed;        /* Number of Colors in Color Table  */
    Bit32u      ClrImportant;   /* Number of Important Colors       */
} WINHDR;

// Logo settings header
typedef struct
{
    Bit16u Signature;
    Bit8u  FadeIn;
    Bit8u  FadeOut;
    Bit16u LogoTime;
    Bit8u  ShowBootMenu;
    Bit32u LogoSize;

} LOGOHDR;

// Width and height of the "Press F12 to select boot device." bitmap. Anything
// that exceeds the limit of F12BootText below is filled with background.
#define F12BOOTTEXTWIDTH 284
#define F12BOOTTEXTHEIGHT 13
// "Press F12 to select boot device." bitmap.
Bit8u F12BootText[] = {
  0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x1F, 0x0C, 0x3E, 0x00, 0x20, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x20, 0x00, 0x70, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x70, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x98, 0xE1, 0x30, 0x06, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
  0xC0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x60, 0x00, 0x00, 0x06,
  0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x11, 0x0F, 0x60, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00,
  0x30, 0x00, 0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x60, 0x66, 0x87, 0x0F, 0x1F, 0x3E, 0x00, 0x58, 0xC0, 0x00, 0x03,
  0xC0, 0x0F, 0x1F, 0x00, 0x7C, 0xF8, 0xC0, 0xE0, 0xC3, 0xC7, 0x0F, 0x00, 0x1E,
  0x7C, 0xF8, 0xF8, 0x01, 0x80, 0x87, 0x8F, 0x61, 0x1C, 0x7C, 0xF8, 0x00, 0x3E,
  0xDC, 0x8C, 0x19, 0x33, 0x06, 0x80, 0x07, 0x0C, 0x18, 0x00, 0x30, 0x18, 0x03,
  0x60, 0xCC, 0x18, 0x0C, 0x63, 0xC6, 0x30, 0x00, 0x60, 0x63, 0xCC, 0x18, 0x06,
  0x00, 0x6C, 0x8C, 0x19, 0x86, 0x61, 0xCC, 0x18, 0x60, 0xC0, 0xCC, 0x1F, 0x03,
  0x06, 0x00, 0x58, 0xC0, 0xC0, 0x00, 0x00, 0x83, 0x31, 0x00, 0x0C, 0xFC, 0xC1,
  0xF0, 0x67, 0x00, 0x03, 0x00, 0x66, 0xC6, 0x8C, 0x61, 0x00, 0x60, 0xC6, 0x9F,
  0x61, 0x18, 0x06, 0xFC, 0x01, 0x06, 0x0C, 0x0C, 0xE0, 0xC0, 0x01, 0x80, 0x01,
  0x0C, 0x06, 0x00, 0x30, 0x18, 0x03, 0x80, 0xC3, 0x00, 0x0C, 0x03, 0x06, 0x30,
  0x00, 0x60, 0x66, 0xCC, 0x18, 0x06, 0x00, 0x66, 0x0C, 0x18, 0x86, 0x61, 0xC0,
  0x00, 0x60, 0xC0, 0xC0, 0x00, 0x18, 0x30, 0x00, 0x18, 0xC0, 0x30, 0x00, 0x00,
  0x83, 0x31, 0x00, 0x60, 0x0C, 0xC0, 0x30, 0x60, 0x00, 0x03, 0x00, 0x66, 0xC6,
  0x8C, 0x61, 0x00, 0x60, 0xC6, 0x00, 0x33, 0x18, 0x06, 0x0C, 0x00, 0x06, 0x0C,
  0x8C, 0x19, 0x33, 0x06, 0x80, 0x01, 0x0C, 0x63, 0x00, 0xB0, 0x19, 0x03, 0x60,
  0xCC, 0x18, 0x0C, 0x63, 0xC6, 0xB0, 0x01, 0x60, 0x66, 0xCC, 0x18, 0x36, 0x00,
  0x66, 0x8C, 0xE1, 0x81, 0x61, 0xCC, 0x18, 0xFC, 0xE0, 0x81, 0x0F, 0x1F, 0x3E,
  0x00, 0x3C, 0xF0, 0xF3, 0x07, 0x00, 0x0E, 0x1F, 0x00, 0x7C, 0xF8, 0xE0, 0xE1,
  0xC3, 0x07, 0x0E, 0x00, 0x3E, 0x7C, 0xF8, 0xC0, 0x01, 0xC0, 0x8D, 0x0F, 0x0C,
  0x3C, 0x7C, 0xF8, 0xC0
};

static unsigned char get_mode();
static void          set_mode();
static Bit8u         wait(ticks, stop_on_key);
static void          write_pixel();
static Bit8u         read_logo_byte();
static Bit16u        read_logo_word();

/**
 * Get current video mode (VGA).
 * @returns    Video mode.
 */
unsigned char get_mode()
  {
  ASM_START
    push bp
    mov  bp, sp

      push bx

      mov  ax, #0x0F00
      int  #0x10

      pop  bx

    pop  bp
  ASM_END
  }

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

void read_palette(pal_seg, bmp_off, size, type)
  Bit16u pal_seg;
  Bit16u bmp_off;
  Bit16u size;
  Bit16u type;
  {
    Bit16u i;
    RGBPAL *palette;

    palette = 0;

    for (i = 0; i < size; i++)
    {
        Bit8u pal;

        pal = read_logo_byte(bmp_off);
        write_byte(pal_seg, &palette->Blue, pal);
        bmp_off++;

        pal = read_logo_byte(bmp_off);
        write_byte(pal_seg, &palette->Green, pal);
        bmp_off++;

        pal = read_logo_byte(bmp_off);
        write_byte(pal_seg, &palette->Red, pal);
        bmp_off++;

        if (type != BMP_HEADER_OS21)
        {
            // Skip 4th byte
            bmp_off++;
        }

        *palette++;
    }
}

void set_dark_palette(palette_size)
  Bit16u palette_size;
  {
    Bit16u i;

    // Set bitmap palette (dark)
    outb(0x03c8, palette_size);

    for (i = 0; i < palette_size; i++)
    {
        outb(0x03c8, i);
        outb(0x03c9, 0);
        outb(0x03c9, 0);
        outb(0x03c9, 0);
    }
}

void set_bitmap_palette(palette_seg, palette_size)
  Bit16u palette_seg;
  Bit16u palette_size;
  {
    RGBPAL *palette;
    Bit16u i;

    palette = 0;

    outb(0x03c6, palette_size);

    for (i = 0; i < palette_size; i++)
    {
        Bit8u b;

        outb(0x03c8, i);

        b = read_byte(palette_seg, &palette->Red);
        outb(0x03c9, b >> 2);
        b = read_byte(palette_seg, &palette->Green);
        outb(0x03c9, b >> 2);
        b = read_byte(palette_seg, &palette->Blue);
        outb(0x03c9, b >> 2);

        *palette++;
    }
}

/**
 * Fade in and check for keystroke.
 * @returns   1 if F12 was pressed, 0 if not.
 */
Bit8u fade_in(palette_seg, palette_size)
  Bit16u palette_seg;
  Bit16u palette_size;
  {
    RGBPAL *palette;
    Bit16u i, j;
    Bit8u  scode;

    // Fade in
    for (i = 0; i < 0x3F; i++)
    {
        outb(0x03c6, palette_size);
        palette = 0;

        for (j = 0; j < palette_size; j++)
        {
            Bit8u r, g, b;

            r = read_byte(palette_seg, &palette->Red) >> 2;
            g = read_byte(palette_seg, &palette->Green) >> 2;
            b = read_byte(palette_seg, &palette->Blue) >> 2;

            if (r > 0 && r >= i) r = i;
            if (g > 0 && g >= i) g = i;
            if (b > 0 && b >= i) b = i;

            outb(0x03c8, j);
            outb(0x03c9, r);
            outb(0x03c9, g);
            outb(0x03c9, b);

            *palette++;
        }
        scode = wait(16 / WAIT_MS, 0);
        if (scode == F12_SCAN_CODE)
            return 1;
    }

    return 0; // F12 not pressed
}

/**
 * Fade out and check for keystroke.
 * @returns    1 if F12 was pressed, 0 if not.
 */
Bit8u fade_out(palette_seg, palette_size)
  Bit16u palette_seg;
  Bit16u palette_size;
  {
    RGBPAL *palette;
    Bit16u i, j;
    Bit8u  scode;

    // Fade out
    for (i = 0x3F; i > 0; i--)
    {
        outb(0x03c6, palette_size);
        palette = 0;

        for (j = 0; j < palette_size; j++)
        {
            Bit8u r, g, b;

            r = read_byte(palette_seg, &palette->Red) >> 2;
            g = read_byte(palette_seg, &palette->Green) >> 2;
            b = read_byte(palette_seg, &palette->Blue) >> 2;

            if (r > 0 && r >= i) r = i;
            if (g > 0 && g >= i) g = i;
            if (b > 0 && b >= i) b = i;

            outb(0x03c8, j);
            outb(0x03c9, r);
            outb(0x03c9, g);
            outb(0x03c9, b);

            *palette++;
        }
        scode = wait(16 / WAIT_MS, 0);
        if (scode == F12_SCAN_CODE)
            return 1;
    }

    return 0; // F12 not pressed
}

void vesa_set_bank(bank)
  Bit16u bank;
  {
  ASM_START
    push bp
    mov  bp, sp

      push bx
      push dx

      mov  ax, #0x4f05
      xor  bx, bx
      mov  dx, 4[bp]    ; bank
      int  #0x10

      pop  dx
      pop  bx

    pop  bp
  ASM_END
}

Bit8u read_logo_byte(offset)
  Bit16u offset;
{
    if (offset)
    {
        outw(LOGO_IO_PORT, offset);
    }

    return inb(LOGO_IO_PORT);
}

Bit16u read_logo_word(offset)
  Bit16u offset;
{
    if (offset)
    {
        outw(LOGO_IO_PORT, offset);
    }

    return inw(LOGO_IO_PORT);
}

#define VID_SEG         0xA000
#define TMP_SEG         0x1000

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
    Bit8u actual_device = 0;
    Bit8u first_ctrl_printed = 0;
    Bit8u second_ctrl_printed = 0;
    Bit8u device;

    device = read_byte(ebda_seg, &EbdaData->ata.hdidmap[actual_device]);

    while ((actual_device < BX_MAX_ATA_DEVICES) && (device < BX_MAX_ATA_DEVICES))
    {
        Bit8u device_position;

        device_position = device;

        if ((device_position < 4) && (first_ctrl_printed == 0))
        {
            printf("IDE controller:\n");
            first_ctrl_printed = 1;
        }
        else if ((device_position >= 4) && (second_ctrl_printed == 0))
        {
            printf("\n\nAHCI controller:\n");
            second_ctrl_printed = 1;
        }

        printf("\n    %d) ", actual_device+1);

        /*
         * If actual_device is bigger than or equal 4
         * this is the next controller and
         * the positions start at the beginning.
         */
        if (device_position >= 4)
            device_position -= 4;

        if (device_position / 2)
            printf("Primary ");
        else
            printf("Secondary ");

        if (device_position % 2)
            printf("Slave");
        else
            printf("Master");

        actual_device++;
        device = read_byte(ebda_seg, &EbdaData->ata.hdidmap[actual_device]);
    }

    printf("\n");
}

Bit8u get_boot_drive(scode)
   Bit8u scode;
{
    Bit16u ebda_seg=read_word(0x0040,0x000E);
    Bit8u actual_device;
    Bit8u detected_devices = 0;

    for (actual_device = 0; actual_device < BX_MAX_ATA_DEVICES; actual_device++)
    {
        Bit8u device = read_byte(ebda_seg, &EbdaData->ata.hdidmap[actual_device]);

        if (device < BX_MAX_ATA_DEVICES)
        {
            scode--;
            if (scode == 0x01)
                return actual_device;
        }
    }

    /* Scancode is higher than number of available devices */
    return 0x08;
}

void show_logo()
{
    Bit16u ebda_seg=read_word(0x0040,0x000E);

    LOGOHDR     *logo_hdr;
    BMPINFO     *bmp_info;
    OS2HDR      *os2_head;
    OS22HDR     *os22_head;
    WINHDR      *win_head;
    Bit16u       rom_seg, pal_seg, logo_hdr_size, tmp, i;
    Bit32u       hdr_size;
    Bit8u        vid_mode;

    Bit8u       is_fade_in, is_fade_out, is_logo_failed, uBootMenu;
    Bit16u      logo_time;

    Bit32u      offset;
    Bit16u      bank = 0;
    Bit8u       logo_bank = 0;
    Bit16u      address;

    Bit8u       scode, f12_pressed = 0;
    Bit8u c;

    // Set PIT to 1ms ticks
    wait_init();

    is_logo_failed = 0;

    logo_hdr = 0;
    logo_hdr_size = sizeof(LOGOHDR);

    // Get main signature
    tmp = read_logo_word(&logo_hdr->Signature);
    if (tmp != 0x66BB)
        goto done;

    // Get options
    is_fade_in = read_logo_byte(&logo_hdr->FadeIn);
    is_fade_out = read_logo_byte(&logo_hdr->FadeOut);
    logo_time = read_logo_word(&logo_hdr->LogoTime);
    uBootMenu = read_logo_byte(&logo_hdr->ShowBootMenu);

    // Is Logo disabled?
    if (!is_fade_in && !is_fade_out && !logo_time)
        goto done;

show_bmp:

    // Set offset of bitmap header
    bmp_info = logo_hdr_size;
    os2_head = os22_head = win_head = logo_hdr_size + sizeof(BMPINFO);

    // Check bitmap ID
    tmp = read_logo_word(&bmp_info->Type);
    if (tmp != 0x4D42) // 'BM'
    {
        goto error;
    }
    else
    {
        Bit16u scr_width, scr_height, start_x, start_y, bmp_data, j;
        Bit16u width, height, compr, clr_used;
        Bit16u pad_bytes, depth, planes, palette_size, palette_data;
        Bit16u bidx, didx;
        int x, y;

        // Check the size of the information header that indicates
        // the structure type
        hdr_size = read_logo_word(&win_head->Size);
        hdr_size |= read_logo_word(0) << 16;

        if (hdr_size == BMP_HEADER_OS21) // OS2 1.x header
        {
            width = read_logo_word(&os2_head->Width);
            height = read_logo_word(&os2_head->Height);
            planes = read_logo_word(&os2_head->Planes);
            depth = read_logo_word(&os2_head->BitCount);
            compr = COMPRESS_NONE;
            clr_used = 0;
        }
        else
        if (hdr_size == BMP_HEADER_OS22) // OS2 2.0 header
        {
            width = read_logo_word(&os22_head->Width);
            height = read_logo_word(&os22_head->Height);
            planes = read_logo_word(&os22_head->Planes);
            depth = read_logo_word(&os22_head->BitCount);
            compr = read_logo_word(&os22_head->Compression);
            clr_used = read_logo_word(&os22_head->ClrUsed);
        }
        else
        if (hdr_size == BMP_HEADER_WIN3) // Windows 3.x header
        {
            width = read_logo_word(&win_head->Width);
            height = read_logo_word(&win_head->Height);
            planes = read_logo_word(&win_head->Planes);
            depth = read_logo_word(&win_head->BitCount);
            compr = read_logo_word(&win_head->Compression);
            clr_used = read_logo_word(&win_head->ClrUsed);
        }
        else
            goto error;

        // Test some bitmap fields
        if (width > 640 || height > 480)
            goto error;

        if (planes != 1)
            goto error;

        if (depth < 4 || depth > 8)
            goto error;

        if (clr_used > 256)
            goto error;

        // Bitmap processing
        if (compr != COMPRESS_NONE)
            goto error;

        // Screen size
        scr_width = 640;
        scr_height = 480;

        // Center of screen
        start_x = (scr_width - width) / 2;
        start_y = (scr_height - height) / 2;

        // Read palette
        if (hdr_size == BMP_HEADER_OS21)
        {
            palette_size = (Bit16u) (1 << (planes * depth));
        }
        else
        if (hdr_size == BMP_HEADER_WIN3 || hdr_size == BMP_HEADER_OS22)
        {
            if (clr_used)
                palette_size = clr_used;
            else
                palette_size = (Bit16u) (1 << (planes * depth));
        }

        pal_seg = TMP_SEG;
        palette_data = logo_hdr_size + sizeof(BMPINFO) + hdr_size;

        read_palette(pal_seg, palette_data, palette_size, hdr_size);

        //  Get current video mode
        vid_mode = get_mode();

        // Set video mode #0x101 640x480x8bpp
        vesa_set_mode(0x101);

        // Set dark/bitmap palette
        if (is_fade_in)
            set_dark_palette(palette_size);
        else
            set_bitmap_palette(pal_seg, palette_size);

        // 0 bank
        vesa_set_bank(0);

        // Show bitmap
        tmp = read_logo_word(&bmp_info->Offset);
        outw(LOGO_IO_PORT, logo_hdr_size + tmp);

        switch(depth)
        {
            case 4:
                // Compute padding bytes
                if (((width % 8) == 0) || ((width % 8) > 6))
                    pad_bytes = 0;
                else if ((width % 8) <= 2)
                    pad_bytes = 3;
                else if ((width % 8) <= 4)
                    pad_bytes = 2;
                else
                    pad_bytes = 1;

                 // For 4 bits per pixel, each byte is two pixels.
                 // The upper half go to the first pixel,
                 // and the lower half to the second.
                for (y = height; y > 0; y--)
                {
                    Bit8u z;

                    for (x = 0; x < width; x += 2)
                    {
                        Bit8u c;

                        c = read_logo_byte(0);

                        for (z = 0; z < 2; z++)
                        {
                            Bit8u color;
                            Bit16u new_bank;

                            offset = (((Bit32u)start_y + (Bit32u)y) * (Bit32u)scr_width) + ((Bit32u)start_x + (Bit32u)x + (Bit32u)z);
                            new_bank = (offset >> 16);
                            address = (Bit16u)(offset & 0xffffL);

                            if (bank != new_bank)
                            {
                                bank = new_bank;
                                vesa_set_bank(bank);
                            }

                            if (z & 1)
                                color = c & 0xF;
                            else
                                color = (c >> 4) & 0xF;

                            write_byte(VID_SEG, address, color);
                        }
                    }

                    for (z = 0; z < pad_bytes; z++)
                    {
                        c = read_logo_byte(0);
                    }
                }
            break;

            case 8:
                // Compute padding bytes
                pad_bytes = ((width % 4) == 0) ? 0 : (4 - (width % 4));

                 // For 8 bits per pixel, each byte is one pixel.
                for (y = height; y > 0; y--)
                {
                    Bit8u z;

                    for (x = 0; x < width; x++)
                    {
                        Bit8u c, z;
                        Bit16u new_bank;

                        c = read_logo_byte(0);

                        offset = (((Bit32u)start_y + (Bit32u)y) * (Bit32u)scr_width) + ((Bit32u)start_x + (Bit32u)x);
                        new_bank = (offset >> 16);
                        address = (Bit16u)(offset & 0xffffL);

                        if (bank != new_bank)
                        {
                            bank = new_bank;
                            vesa_set_bank(bank);
                        }

                        write_byte(VID_SEG, address, c);
                    }

                    for (z = 0; z < pad_bytes; z++)
                    {
                        c = read_logo_byte(0);
                    }
                }
            break;

#if 0 // 24bpp bitmaps are unsupported
            case 24:
                // Compute padding bytes
                pad_bytes = width % 4;

                // For 24 bits per pixel it's RGB structure.
                for (y = height; y > 0; y--)
                {
                    Bit8u z;
                    for (x = 0; x < width; x++)
                    {
                        for (z = 0; z < 3; z++)
                        {
                            Bit8u color;
                            Bit16u new_bank;

                            color = read_logo_byte(0);

                            offset = (((Bit32u)start_y + (Bit32u)y) * (Bit32u)scr_width*3) + (((Bit32u)start_x + (Bit32u)x) * (Bit32u)3 + z);
                            new_bank = (offset >> 16);
                            address = (Bit16u)(offset & 0xffffL);

                            if (bank != new_bank)
                            {
                                bank = new_bank;
                                vesa_set_bank(bank);
                            }

                            write_byte(VID_SEG, address, color);
                        }
                    }

                    for (z = 0; z < pad_bytes; z++)
                    {
                        c = read_logo_byte(0);
                    }
                }
            break;
#endif
        }

        // If Setup menu enabled
        if (uBootMenu == 2 && (is_fade_in || is_fade_out || logo_time))
        {
            RGBPAL *palette = 0;
            Bit16u blum, dlum;

            // Get the brightest and the darkest palette indexes
            bidx = didx = blum = 0;
            dlum = 3 * 0xff;

            for (i = 0; i < palette_size; i++)
            {
                Bit8u r, g, b;
                Bit16u lum;

                r = read_byte(pal_seg, &palette->Red) >> 2;
                g = read_byte(pal_seg, &palette->Green) >> 2;
                b = read_byte(pal_seg, &palette->Blue) >> 2;
                lum = (Bit16u)r + (Bit16u)g + (Bit16u)b;

                if (lum > blum) { blum = lum; bidx = i; }

                if (lum < dlum) { dlum = lum; didx = i; }

                *palette++;
            }

            // 0 bank
            vesa_set_bank(0);

            // Top-left corner of screen
            start_x = 340;
            start_y = 450;

            // Image size
            width = (start_x + F12BOOTTEXTWIDTH <= scr_width) ? F12BOOTTEXTWIDTH : scr_width - start_x;
            height = (start_y + F12BOOTTEXTHEIGHT <= scr_height) ? F12BOOTTEXTHEIGHT : scr_height - start_y;
            bmp_data = j = 0;

            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    Bit16u new_bank;
                    Bit8u pix_idx;

                    if (!j)
                    {
                        if (bmp_data < sizeof(F12BootText))
                            c = read_byte(0xf000, F12BootText + bmp_data++);
                        else
                            c = 0;
                    }

                    offset = (((Bit32u)start_y + (Bit32u)y) * (Bit32u)scr_width) + ((Bit32u)start_x + (Bit32u)x);
                    new_bank = (offset >> 16);
                    address = (Bit16u)(offset & 0xffffL);

                    if (bank != new_bank)
                    {
                        bank = new_bank;
                        vesa_set_bank(bank);
                    }

                    pix_idx = c & 1;
                    c >>= 1;

                    if (pix_idx)
                        pix_idx = bidx;
                    else
                        pix_idx = didx;

                    write_byte(VID_SEG, address, pix_idx);

                    if (j++ >= 7) j = 0;
                }
            }
        }

        // Fade in
        if (is_fade_in)
        {
            if (fade_in(pal_seg, palette_size))
                f12_pressed = 1;
        }

        // Wait (interval in milliseconds)
        if (!f12_pressed)
        {
            scode = wait(logo_time / WAIT_MS, 0);
            if (scode == F12_SCAN_CODE)
                f12_pressed = 1;
        }

        // Fade out (only if F12 was not pressed)
        if (is_fade_out && !f12_pressed)
        {
            if (fade_out(pal_seg, palette_size))
                f12_pressed = 1;
        }
    }

    goto done;

error:
    if (!is_logo_failed)
    {
        is_logo_failed = 1;

        logo_hdr_size = 0;

        // Switch to defaul logo
        outw(LOGO_IO_PORT, 0xFFFF);

        goto show_bmp;
    }
done:

    // Clear forced boot drive setting.
    write_byte(ebda_seg,&EbdaData->uForceBootDevice, 0);

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
                printf("Press F12 to select boot device.");

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

            // Show menu
            printf("\n"
                   "VirtualBox temporary boot device selection\n"
                   "\n"
                   "Detected Hard disks:\n"
                   "\n");
            print_detected_harddisks();
            printf("\n"
                   "Other boot devices:\n"
                   " f) Floppy\n"
                   " c) CD-ROM\n"
                   " l) LAN\n"
                   "\n"
                   " b) Continue booting\n");



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
                     * We support a maximum of 8 boot drives.
                     * If this value is bigger than 7 not all
                     * values are used and the user pressed
                     * and invalid key.
                     * Wait for the next pressed key.
                     */
                    if (boot_drive > 7)
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

