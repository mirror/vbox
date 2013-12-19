/*
 * VMware SVGA II hardware definitions
 */

#ifndef _SVGA_REG_H_
#define _SVGA_REG_H_

#define PCI_VENDOR_ID_VMWARE            0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2      0x0405

#define SVGA_IRQFLAG_ANY_FENCE            0x1
#define SVGA_IRQFLAG_FIFO_PROGRESS        0x2
#define SVGA_IRQFLAG_FENCE_GOAL           0x4

#define SVGA_MAX_PSEUDOCOLOR_DEPTH      8
#define SVGA_MAX_PSEUDOCOLORS           (1 << SVGA_MAX_PSEUDOCOLOR_DEPTH)
#define SVGA_NUM_PALETTE_REGS           (3 * SVGA_MAX_PSEUDOCOLORS)

#define SVGA_MAGIC         0x900000UL
#define SVGA_MAKE_ID(ver)  (SVGA_MAGIC << 8 | (ver))

#define SVGA_VERSION_2     2
#define SVGA_ID_2          SVGA_MAKE_ID(SVGA_VERSION_2)

#define SVGA_VERSION_1     1
#define SVGA_ID_1          SVGA_MAKE_ID(SVGA_VERSION_1)

#define SVGA_VERSION_0     0
#define SVGA_ID_0          SVGA_MAKE_ID(SVGA_VERSION_0)

#define SVGA_ID_INVALID    0xFFFFFFFF

#define SVGA_INDEX_PORT         0x0
#define SVGA_VALUE_PORT         0x1
#define SVGA_BIOS_PORT          0x2
#define SVGA_IRQSTATUS_PORT     0x8

#define SVGA_IRQFLAG_ANY_FENCE            0x1
#define SVGA_IRQFLAG_FIFO_PROGRESS        0x2
#define SVGA_IRQFLAG_FENCE_GOAL           0x4

enum
{
   SVGA_REG_ID = 0,
   SVGA_REG_ENABLE = 1,
   SVGA_REG_WIDTH = 2,
   SVGA_REG_HEIGHT = 3,
   SVGA_REG_MAX_WIDTH = 4,
   SVGA_REG_MAX_HEIGHT = 5,
   SVGA_REG_DEPTH = 6,
   SVGA_REG_BITS_PER_PIXEL = 7,
   SVGA_REG_PSEUDOCOLOR = 8,
   SVGA_REG_RED_MASK = 9,
   SVGA_REG_GREEN_MASK = 10,
   SVGA_REG_BLUE_MASK = 11,
   SVGA_REG_BYTES_PER_LINE = 12,
   SVGA_REG_FB_START = 13,
   SVGA_REG_FB_OFFSET = 14,
   SVGA_REG_VRAM_SIZE = 15,
   SVGA_REG_FB_SIZE = 16,
   SVGA_REG_CAPABILITIES = 17,
   SVGA_REG_MEM_START = 18,
   SVGA_REG_MEM_SIZE = 19,
   SVGA_REG_CONFIG_DONE = 20,
   SVGA_REG_SYNC = 21,
   SVGA_REG_BUSY = 22,
   SVGA_REG_GUEST_ID = 23,
   SVGA_REG_CURSOR_ID = 24,
   SVGA_REG_CURSOR_X = 25,
   SVGA_REG_CURSOR_Y = 26,
   SVGA_REG_CURSOR_ON = 27,
   SVGA_REG_HOST_BITS_PER_PIXEL = 28,
   SVGA_REG_SCRATCH_SIZE = 29,
   SVGA_REG_MEM_REGS = 30,
   SVGA_REG_NUM_DISPLAYS = 31,
   SVGA_REG_PITCHLOCK = 32,
   SVGA_REG_IRQMASK = 33,
   SVGA_REG_NUM_GUEST_DISPLAYS = 34,
   SVGA_REG_DISPLAY_ID = 35,
   SVGA_REG_DISPLAY_IS_PRIMARY = 36,
   SVGA_REG_DISPLAY_POSITION_X = 37,
   SVGA_REG_DISPLAY_POSITION_Y = 38,
   SVGA_REG_DISPLAY_WIDTH = 39,
   SVGA_REG_DISPLAY_HEIGHT = 40,
   SVGA_REG_GMR_ID = 41,
   SVGA_REG_GMR_DESCRIPTOR = 42,
   SVGA_REG_GMR_MAX_IDS = 43,
   SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH = 44,
   SVGA_REG_TRACES = 45,
   SVGA_REG_GMRS_MAX_PAGES = 46,
   SVGA_REG_MEMORY_SIZE = 47,
   SVGA_REG_TOP = 48,
   SVGA_PALETTE_BASE = 1024,
   SVGA_SCRATCH_BASE = SVGA_PALETTE_BASE + SVGA_NUM_PALETTE_REGS
};

enum
{
   SVGA_FIFO_MIN = 0,
   SVGA_FIFO_MAX,
   SVGA_FIFO_NEXT_CMD,
   SVGA_FIFO_STOP,
   SVGA_FIFO_CAPABILITIES = 4,
   SVGA_FIFO_FLAGS,
   SVGA_FIFO_FENCE,
   SVGA_FIFO_3D_HWVERSION,
   SVGA_FIFO_PITCHLOCK,
   SVGA_FIFO_CURSOR_ON,
   SVGA_FIFO_CURSOR_X,
   SVGA_FIFO_CURSOR_Y,
   SVGA_FIFO_CURSOR_COUNT,
   SVGA_FIFO_CURSOR_LAST_UPDATED,
   SVGA_FIFO_RESERVED,
   SVGA_FIFO_CURSOR_SCREEN_ID,
   SVGA_FIFO_DEAD,
   SVGA_FIFO_3D_HWVERSION_REVISED,
   SVGA_FIFO_3D_CAPS      = 32,
   SVGA_FIFO_3D_CAPS_LAST = 32 + 255,
   SVGA_FIFO_GUEST_3D_HWVERSION,
   SVGA_FIFO_FENCE_GOAL,
   SVGA_FIFO_BUSY,
   SVGA_FIFO_NUM_REGS
};

typedef enum
{
   SVGA_CMD_INVALID_CMD           = 0,
   SVGA_CMD_UPDATE                = 1,
   SVGA_CMD_RECT_COPY             = 3,
   SVGA_CMD_DEFINE_CURSOR         = 19,
   SVGA_CMD_DEFINE_ALPHA_CURSOR   = 22,
   SVGA_CMD_UPDATE_VERBOSE        = 25,
   SVGA_CMD_FRONT_ROP_FILL        = 29,
   SVGA_CMD_FENCE                 = 30,
   SVGA_CMD_ESCAPE                = 33,
   SVGA_CMD_DEFINE_SCREEN         = 34,
   SVGA_CMD_DESTROY_SCREEN        = 35,
   SVGA_CMD_DEFINE_GMRFB          = 36,
   SVGA_CMD_BLIT_GMRFB_TO_SCREEN  = 37,
   SVGA_CMD_BLIT_SCREEN_TO_GMRFB  = 38,
   SVGA_CMD_ANNOTATION_FILL       = 39,
   SVGA_CMD_ANNOTATION_COPY       = 40,
   SVGA_CMD_DEFINE_GMR2           = 41,
   SVGA_CMD_REMAP_GMR2            = 42,
   SVGA_CMD_MAX
} SVGAFifoCmdId;

typedef struct SVGAColorBGRX
{
   union
   {
      struct
      {
         uint32_t b : 8;
         uint32_t g : 8;
         uint32_t r : 8;
         uint32_t x : 8;
      } s;

      uint32_t value;
   };
} SVGAColorBGRX;

typedef struct SVGASignedPoint
{
   int32_t  x;
   int32_t  y;
} SVGASignedPoint;

#define SVGA_CAP_NONE               0x00000000
#define SVGA_CAP_RECT_COPY          0x00000002
#define SVGA_CAP_CURSOR             0x00000020
#define SVGA_CAP_CURSOR_BYPASS      0x00000040
#define SVGA_CAP_CURSOR_BYPASS_2    0x00000080
#define SVGA_CAP_8BIT_EMULATION     0x00000100
#define SVGA_CAP_ALPHA_CURSOR       0x00000200
#define SVGA_CAP_3D                 0x00004000
#define SVGA_CAP_EXTENDED_FIFO      0x00008000
#define SVGA_CAP_MULTIMON           0x00010000
#define SVGA_CAP_PITCHLOCK          0x00020000
#define SVGA_CAP_IRQMASK            0x00040000
#define SVGA_CAP_DISPLAY_TOPOLOGY   0x00080000
#define SVGA_CAP_GMR                0x00100000
#define SVGA_CAP_TRACES             0x00200000
#define SVGA_CAP_GMR2               0x00400000
#define SVGA_CAP_SCREEN_OBJECT_2    0x00800000

#define SVGA_GMR_NULL         ((uint32_t) -1)
#define SVGA_GMR_FRAMEBUFFER  ((uint32_t) -2)

typedef struct SVGAGuestPtr
{
   uint32_t gmrId;
   uint32_t offset;
} SVGAGuestPtr;

typedef struct SVGAGMRImageFormat
{
   union
   {
      struct
      {
         uint32_t bitsPerPixel : 8;
         uint32_t colorDepth   : 8;
         uint32_t reserved     : 16;
      } s;

      uint32_t value;
   };
} SVGAGMRImageFormat;

typedef struct SVGAGuestImage
{
   SVGAGuestPtr         ptr;
   uint32_t pitch;
} SVGAGuestImage;

#define SVGA_SCREEN_MUST_BE_SET     (1 << 0)
#define SVGA_SCREEN_HAS_ROOT SVGA_SCREEN_MUST_BE_SET
#define SVGA_SCREEN_IS_PRIMARY      (1 << 1)
#define SVGA_SCREEN_FULLSCREEN_HINT (1 << 2)
#define SVGA_SCREEN_DEACTIVATE  (1 << 3)
#define SVGA_SCREEN_BLANKING (1 << 4)

typedef struct SVGAScreenObject
{
   uint32_t structSize;
   uint32_t id;
   uint32_t flags;
   struct
   {
      uint32_t width;
      uint32_t height;
   } size;
   struct
   {
      int32_t x;
      int32_t y;
   } root;
   SVGAGuestImage backingStore;
   uint32_t cloneCount;
} SVGAScreenObject;

typedef struct
{
   uint32_t screenId;
} SVGAFifoCmdDestroyScreen;

typedef struct
{
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} SVGAFifoCmdUpdate;

typedef struct
{
   uint32_t fence;
} SVGAFifoCmdFence;

typedef struct
{
   uint32_t nsid;
   uint32_t size;
} SVGAFifoCmdEscape;

typedef struct
{
   uint32_t id;
   uint32_t hotspotX;
   uint32_t hotspotY;
   uint32_t width;
   uint32_t height;
   uint32_t andMaskDepth;
   uint32_t xorMaskDepth;
} SVGAFifoCmdDefineCursor;

typedef struct
{
   uint32_t id;
   uint32_t hotspotX;
   uint32_t hotspotY;
   uint32_t width;
   uint32_t height;
} SVGAFifoCmdDefineAlphaCursor;

typedef struct
{
   SVGAScreenObject screen;
} SVGAFifoCmdDefineScreen;

typedef struct
{
   SVGAColorBGRX  color;
} SVGAFifoCmdAnnotationFill;

typedef struct
{
   SVGASignedPoint  srcOrigin;
   uint32_t           srcScreenId;
} SVGAFifoCmdAnnotationCopy;

#define SVGA_FIFO_CAP_NONE                  0
#define SVGA_FIFO_CAP_FENCE             (1<<0)
#define SVGA_FIFO_CAP_ACCELFRONT        (1<<1)
#define SVGA_FIFO_CAP_PITCHLOCK         (1<<2)
#define SVGA_FIFO_CAP_VIDEO             (1<<3)
#define SVGA_FIFO_CAP_CURSOR_BYPASS_3   (1<<4)
#define SVGA_FIFO_CAP_ESCAPE            (1<<5)
#define SVGA_FIFO_CAP_RESERVE           (1<<6)
#define SVGA_FIFO_CAP_SCREEN_OBJECT     (1<<7)
#define SVGA_FIFO_CAP_GMR2              (1<<8)
#define SVGA_FIFO_CAP_3D_HWVERSION_REVISED  SVGA_FIFO_CAP_GMR2
#define SVGA_FIFO_CAP_SCREEN_OBJECT_2   (1<<9)
#define SVGA_FIFO_CAP_DEAD              (1<<10)

#endif /* _SVGA_REG_H_ */
