/*
 * VMware SVGA video overlay definitions
 */

#ifndef _SVGA_OVERLAY_H_
#define _SVGA_OVERLAY_H_

#define SVGA_ESCAPE_VMWARE_VIDEO_SET_REGS    0x00020001
#define SVGA_ESCAPE_VMWARE_VIDEO_FLUSH       0x00020002

typedef struct SVGAEscapeVideoSetRegs
{
   struct
   {
      uint32_t cmdType;
      uint32_t streamId;
   } header;

   struct
   {
      uint32_t registerId;
      uint32_t value;
   } items[1];
} SVGAEscapeVideoSetRegs;

typedef struct SVGAEscapeVideoFlush
{
   uint32_t cmdType;
   uint32_t streamId;
} SVGAEscapeVideoFlush;

#endif /* _SVGA_OVERLAY_H_ */
