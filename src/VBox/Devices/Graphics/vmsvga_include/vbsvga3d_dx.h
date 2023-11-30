/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_vmsvga_include_vbsvga3d_dx_h
#define VBOX_INCLUDED_SRC_Graphics_vmsvga_include_vbsvga3d_dx_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "svga3d_dx.h"

/* Extended capabilities returned by SVGA3D_DEVCAP_3D if VBoxSVGA virtual device is enabled. */
/* The original "3D support" capability. */
#define VBSVGA3D_CAP_3D    0x00000001
/* Video decoding/processing and ClearView commands. */
#define VBSVGA3D_CAP_VIDEO 0x00000002

/* Arbitrary limits. Allows to use constant size structures.
 * NVIDIA supports 5 streams, AMD more, so 8 seems to be a good round number.
 * Both support 1 rate conversion caps set.
 * NVIDIA driver reports 6 custom rates.
 */
#define VBSVGA3D_MAX_VIDEO_STREAMS 8
#define VBSVGA3D_MAX_VIDEO_RATE_CONVERSION_CAPS 1
#define VBSVGA3D_MAX_VIDEO_CUSTOM_RATE_CAPS 8

/* For 8-bit palettized formats. */
#define VBSVGA3D_MAX_VIDEO_PALETTE_ENTRIES 256

typedef uint32 VBSVGA3dVideoProcessorId;
typedef uint32 VBSVGA3dVideoDecoderOutputViewId;
typedef uint32 VBSVGA3dVideoDecoderId;
typedef uint32 VBSVGA3dVideoProcessorInputViewId;
typedef uint32 VBSVGA3dVideoProcessorOutputViewId;

#define VBSVGA3D_VIDEO_FRAME_FORMAT_PROGRESSIVE                   0
#define VBSVGA3D_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST    1
#define VBSVGA3D_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST 2
typedef uint32 VBSVGA3dVideoFrameFormat;

#define VBSVGA3D_VIDEO_USAGE_PLAYBACK_NORMAL 0
#define VBSVGA3D_VIDEO_USAGE_OPTIMAL_SPEED   1
#define VBSVGA3D_VIDEO_USAGE_OPTIMAL_QUALITY 2
typedef uint32 VBSVGA3dVideoUsage;

#define VBSVGA3D_VP_OUTPUT_RATE_NORMAL 0
#define VBSVGA3D_VP_OUTPUT_RATE_HALF   1
#define VBSVGA3D_VP_OUTPUT_RATE_CUSTOM 2
typedef uint32 VBSVGA3dVideoProcessorOutputRate;

#define VBSVGA3D_VDOV_DIMENSION_UNKNOWN   0
#define VBSVGA3D_VDOV_DIMENSION_TEXTURE2D 1
typedef uint32 VBSVGA3dVDOVDimension;

#define VBSVGA3D_VPIV_DIMENSION_UNKNOWN   0
#define VBSVGA3D_VPIV_DIMENSION_TEXTURE2D 1
typedef uint32 VBSVGA3dVPIVDimension;

#define VBSVGA3D_VPOV_DIMENSION_UNKNOWN        0
#define VBSVGA3D_VPOV_DIMENSION_TEXTURE2D      1
#define VBSVGA3D_VPOV_DIMENSION_TEXTURE2DARRAY 2
typedef uint32 VBSVGA3dVPOVDimension;

#define VBSVGA3D_VD_BUFFER_PICTURE_PARAMETERS          0
#define VBSVGA3D_VD_BUFFER_MACROBLOCK_CONTROL          1
#define VBSVGA3D_VD_BUFFER_RESIDUAL_DIFFERENCE         2
#define VBSVGA3D_VD_BUFFER_DEBLOCKING_CONTROL          3
#define VBSVGA3D_VD_BUFFER_INVERSE_QUANTIZATION_MATRIX 4
#define VBSVGA3D_VD_BUFFER_SLICE_CONTROL               5
#define VBSVGA3D_VD_BUFFER_BITSTREAM                   6
#define VBSVGA3D_VD_BUFFER_MOTION_VECTOR               7
#define VBSVGA3D_VD_BUFFER_FILM_GRAIN                  8
typedef uint32 VBSVGA3dVideoDecoderBufferType;

#define VBSVGA3D_VP_ALPHA_FILL_MODE_OPAQUE        0
#define VBSVGA3D_VP_ALPHA_FILL_MODE_BACKGROUND    1
#define VBSVGA3D_VP_ALPHA_FILL_MODE_DESTINATION   2
#define VBSVGA3D_VP_ALPHA_FILL_MODE_SOURCE_STREAM 3
typedef uint32 VBSVGA3dVideoProcessorAlphaFillMode;

#define VBSVGA3D_VP_STEREO_FORMAT_MONO               0
#define VBSVGA3D_VP_STEREO_FORMAT_HORIZONTAL         1
#define VBSVGA3D_VP_STEREO_FORMAT_VERTICAL           2
#define VBSVGA3D_VP_STEREO_FORMAT_SEPARATE           3
#define VBSVGA3D_VP_STEREO_FORMAT_MONO_OFFSET        4
#define VBSVGA3D_VP_STEREO_FORMAT_ROW_INTERLEAVED    5
#define VBSVGA3D_VP_STEREO_FORMAT_COLUMN_INTERLEAVED 6
#define VBSVGA3D_VP_STEREO_FORMAT_CHECKERBOARD       7
typedef uint32 VBSVGA3dVideoProcessorStereoFormat;

#define VBSVGA3D_VP_STEREO_FLIP_NONE   0
#define VBSVGA3D_VP_STEREO_FLIP_FRAME0 1
#define VBSVGA3D_VP_STEREO_FLIP_FRAME1 2
typedef uint32 VBSVGA3dVideoProcessorStereoFlipMode;

#define VBSVGA3D_VP_FILTER_BRIGHTNESS         0
#define VBSVGA3D_VP_FILTER_CONTRAST           1
#define VBSVGA3D_VP_FILTER_HUE                2
#define VBSVGA3D_VP_FILTER_SATURATION         3
#define VBSVGA3D_VP_FILTER_NOISE_REDUCTION    4
#define VBSVGA3D_VP_FILTER_EDGE_ENHANCEMENT   5
#define VBSVGA3D_VP_FILTER_ANAMORPHIC_SCALING 6
#define VBSVGA3D_VP_FILTER_STEREO_ADJUSTMENT  7
#define VBSVGA3D_VP_MAX_FILTER_COUNT          8
typedef uint32 VBSVGA3dVideoProcessorFilter;

#define VBSVGA3D_VP_ROTATION_IDENTITY 0
#define VBSVGA3D_VP_ROTATION_90       1
#define VBSVGA3D_VP_ROTATION_180      2
#define VBSVGA3D_VP_ROTATION_270      3
typedef uint32 VBSVGA3dVideoProcessorRotation;

#define VBSVGA3D_VP_FORMAT_SUPPORT_INPUT  0x1
#define VBSVGA3D_VP_FORMAT_SUPPORT_OUTPUT 0x2
typedef uint8 VBSVGA3dVideoProcessorFormatSupport;

#define VBSVGA3D_VP_DEVICE_CAPS_LINEAR_SPACE            0x1
#define VBSVGA3D_VP_DEVICE_CAPS_xvYCC                   0x2
#define VBSVGA3D_VP_DEVICE_CAPS_RGB_RANGE_CONVERSION    0x4
#define VBSVGA3D_VP_DEVICE_CAPS_YCbCr_MATRIX_CONVERSION 0x8
#define VBSVGA3D_VP_DEVICE_CAPS_NOMINAL_RANGE           0x10
typedef uint32 VBSVGA3dVideoProcessorDeviceCaps;

#define VBSVGA3D_VP_FEATURE_CAPS_ALPHA_FILL         0x1
#define VBSVGA3D_VP_FEATURE_CAPS_CONSTRICTION       0x2
#define VBSVGA3D_VP_FEATURE_CAPS_LUMA_KEY           0x4
#define VBSVGA3D_VP_FEATURE_CAPS_ALPHA_PALETTE      0x8
#define VBSVGA3D_VP_FEATURE_CAPS_LEGACY             0x10
#define VBSVGA3D_VP_FEATURE_CAPS_STEREO             0x20
#define VBSVGA3D_VP_FEATURE_CAPS_ROTATION           0x40
#define VBSVGA3D_VP_FEATURE_CAPS_ALPHA_STREAM       0x80
#define VBSVGA3D_VP_FEATURE_CAPS_PIXEL_ASPECT_RATIO 0x100
#define VBSVGA3D_VP_FEATURE_CAPS_MIRROR             0x200
#define VBSVGA3D_VP_FEATURE_CAPS_SHADER_USAGE       0x400
#define VBSVGA3D_VP_FEATURE_CAPS_METADATA_HDR10     0x800
typedef uint32 VBSVGA3dVideoProcessorFeatureCaps;

#define VBSVGA3D_VP_FILTER_CAPS_BRIGHTNESS         0x1
#define VBSVGA3D_VP_FILTER_CAPS_CONTRAST           0x2
#define VBSVGA3D_VP_FILTER_CAPS_HUE                0x4
#define VBSVGA3D_VP_FILTER_CAPS_SATURATION         0x8
#define VBSVGA3D_VP_FILTER_CAPS_NOISE_REDUCTION    0x10
#define VBSVGA3D_VP_FILTER_CAPS_EDGE_ENHANCEMENT   0x20
#define VBSVGA3D_VP_FILTER_CAPS_ANAMORPHIC_SCALING 0x40
#define VBSVGA3D_VP_FILTER_CAPS_STEREO_ADJUSTMENT  0x80
typedef uint32 VBSVGA3dVideoProcessorFilterCaps;

#define VBSVGA3D_VP_FORMAT_CAPS_RGB_INTERLACED     0x1
#define VBSVGA3D_VP_FORMAT_CAPS_RGB_PROCAMP        0x2
#define VBSVGA3D_VP_FORMAT_CAPS_RGB_LUMA_KEY       0x4
#define VBSVGA3D_VP_FORMAT_CAPS_PALETTE_INTERLACED 0x8
typedef uint32 VBSVGA3dVideoProcessorInputFormatCaps;

#define VBSVGA3D_VP_AUTO_STREAM_CAPS_DENOISE             0x1
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_DERINGING           0x2
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_EDGE_ENHANCEMENT    0x4
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_COLOR_CORRECTION    0x8
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_FLESH_TONE_MAPPING  0x10
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_IMAGE_STABILIZATION 0x20
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_SUPER_RESOLUTION    0x40
#define VBSVGA3D_VP_AUTO_STREAM_CAPS_ANAMORPHIC_SCALING  0x80
typedef uint32 VBSVGA3dVideoProcessorAutoStreamCaps;

#define VBSVGA3D_VP_STEREO_CAPS_MONO_OFFSET        0x1
#define VBSVGA3D_VP_STEREO_CAPS_ROW_INTERLEAVED    0x2
#define VBSVGA3D_VP_STEREO_CAPS_COLUMN_INTERLEAVED 0x4
#define VBSVGA3D_VP_STEREO_CAPS_CHECKERBOARD       0x8
#define VBSVGA3D_VP_STEREO_CAPS_FLIP_MODE          0x10
typedef uint32 VBSVGA3dVideoProcessorStereoCaps;

#define VBSVGA3D_VP_CAPS_DEINTERLACE_BLEND                 0x1
#define VBSVGA3D_VP_CAPS_DEINTERLACE_BOB                   0x2
#define VBSVGA3D_VP_CAPS_DEINTERLACE_ADAPTIVE              0x4
#define VBSVGA3D_VP_CAPS_DEINTERLACE_MOTION_COMPENSATION   0x8
#define VBSVGA3D_VP_CAPS_INVERSE_TELECINE                  0x10
#define VBSVGA3D_VP_CAPS_FRAME_RATE_CONVERSION             0x20
typedef uint32 VBSVGA3dVideoRateConversionProcessorCaps;

#define VBSVGA3D_VP_ITELECINE_CAPS_32           0x1
#define VBSVGA3D_VP_ITELECINE_CAPS_22           0x2
#define VBSVGA3D_VP_ITELECINE_CAPS_2224         0x4
#define VBSVGA3D_VP_ITELECINE_CAPS_2332         0x8
#define VBSVGA3D_VP_ITELECINE_CAPS_32322        0x10
#define VBSVGA3D_VP_ITELECINE_CAPS_55           0x20
#define VBSVGA3D_VP_ITELECINE_CAPS_64           0x40
#define VBSVGA3D_VP_ITELECINE_CAPS_87           0x80
#define VBSVGA3D_VP_ITELECINE_CAPS_222222222223 0x100
#define VBSVGA3D_VP_ITELECINE_CAPS_OTHER        0x80000000
typedef uint32 VBSVGA3dVideoRateConversionITelecineCaps;

#define VBSVGA3D_VIDEO_CAPABILITY_DECODE_PROFILE 0
#define VBSVGA3D_VIDEO_CAPABILITY_DECODE_CONFIG  1
#define VBSVGA3D_VIDEO_CAPABILITY_PROCESSOR_ENUM 2
typedef uint32 VBSVGA3dVideoCapability;

typedef struct {
    union {
        float r;
        float y;
    };
    union {
        float g;
        float cb;
    };
    union {
        float b;
        float cr;
    };
    float a;
} VBSVGA3dVideoColor;

typedef struct {
    uint32 Usage : 1;
    uint32 RGB_Range : 1;
    uint32 YCbCr_Matrix : 1;
    uint32 YCbCr_xvYCC : 1;
    uint32 Nominal_Range : 2;
    uint32 Reserved : 26;
} VBSVGA3dVideoProcessorColorSpace;

typedef struct {
    VBSVGA3dVideoFrameFormat InputFrameFormat;
    SVGA3dFraction64 InputFrameRate;
    uint32 InputWidth;
    uint32 InputHeight;
    SVGA3dFraction64 OutputFrameRate;
    uint32 OutputWidth;
    uint32 OutputHeight;
    VBSVGA3dVideoUsage Usage;
} VBSVGA3dVideoProcessorDesc;

#define VBSVGA3D_VP_SET_STREAM_FRAME_FORMAT         0x00000001
#define VBSVGA3D_VP_SET_STREAM_COLOR_SPACE          0x00000002
#define VBSVGA3D_VP_SET_STREAM_OUTPUT_RATE          0x00000004
#define VBSVGA3D_VP_SET_STREAM_SOURCE_RECT          0x00000008
#define VBSVGA3D_VP_SET_STREAM_DEST_RECT            0x00000010
#define VBSVGA3D_VP_SET_STREAM_ALPHA                0x00000020
#define VBSVGA3D_VP_SET_STREAM_PALETTE              0x00000040
#define VBSVGA3D_VP_SET_STREAM_ASPECT_RATIO         0x00000080
#define VBSVGA3D_VP_SET_STREAM_LUMA_KEY             0x00000100
#define VBSVGA3D_VP_SET_STREAM_STEREO_FORMAT        0x00000200
#define VBSVGA3D_VP_SET_STREAM_AUTO_PROCESSING_MODE 0x00000400
#define VBSVGA3D_VP_SET_STREAM_FILTER               0x00000800
#define VBSVGA3D_VP_SET_STREAM_ROTATION             0x00001000
typedef uint32 VBSVGA3dVideoProcessorStreamSetMask;

typedef struct
{
    VBSVGA3dVideoProcessorStreamSetMask SetMask;

    uint32 SourceRectEnable : 1;
    uint32 DestRectEnable : 1;
    uint32 AlphaEnable : 1;
    uint32 AspectRatioEnable : 1;
    uint32 LumaKeyEnable : 1;
    uint32 StereoFormatEnable : 1;
    uint32 AutoProcessingModeEnable : 1;
    uint32 RotationEnable : 1;

    VBSVGA3dVideoFrameFormat FrameFormat;
    VBSVGA3dVideoProcessorColorSpace ColorSpace;
    VBSVGA3dVideoProcessorOutputRate OutputRate;
    uint32 RepeatFrame;
    SVGA3dFraction64 CustomRate;
    SVGASignedRect SourceRect;
    SVGASignedRect DestRect;
    float Alpha;
    uint32 PaletteCount;
    uint32 aPalette[VBSVGA3D_MAX_VIDEO_PALETTE_ENTRIES];
    SVGA3dFraction64 AspectSourceRatio;
    SVGA3dFraction64 AspectDestRatio;
    float LumaKeyLower;
    float LumaKeyUpper;
    VBSVGA3dVideoProcessorStereoFormat StereoFormat;
    uint32 LeftViewFrame0 : 1;
    uint32 BaseViewFrame0 : 1;
    VBSVGA3dVideoProcessorStereoFlipMode FlipMode;
    int32 MonoOffset;
    uint32 FilterEnableMask;
    struct {
        int32 Level;
    } aFilter[VBSVGA3D_VP_MAX_FILTER_COUNT];
    VBSVGA3dVideoProcessorRotation Rotation;
} VBSVGA3dVideoProcessorStreamState;

#define VBSVGA3D_VP_SET_OUTPUT_TARGET_RECT       0x00000001
#define VBSVGA3D_VP_SET_OUTPUT_BACKGROUND_COLOR  0x00000002
#define VBSVGA3D_VP_SET_OUTPUT_COLOR_SPACE       0x00000004
#define VBSVGA3D_VP_SET_OUTPUT_ALPHA_FILL_MODE   0x00000008
#define VBSVGA3D_VP_SET_OUTPUT_CONSTRICTION      0x00000010
#define VBSVGA3D_VP_SET_OUTPUT_STEREO_MODE       0x00000020
typedef uint32 VBSVGA3dVideoProcessorOutputSetMask;

typedef struct
{
    VBSVGA3dVideoProcessorOutputSetMask SetMask;

    uint32 TargetRectEnable : 1;
    uint32 BackgroundColorYCbCr : 1;
    uint32 ConstrictionEnable : 1;
    uint32 StereoModeEnable : 1;

    SVGASignedRect TargetRect;
    VBSVGA3dVideoColor BackgroundColor;
    VBSVGA3dVideoProcessorColorSpace ColorSpace;
    VBSVGA3dVideoProcessorAlphaFillMode AlphaFillMode;
    uint32 AlphaFillStreamIndex;
    uint32 ConstrictionWidth;
    uint32 ConstrictionHeight;
} VBSVGA3dVideoProcessorOutputState;

typedef struct {
    VBSVGA3dVideoProcessorDesc desc;

    VBSVGA3dVideoProcessorOutputState output;

    VBSVGA3dVideoProcessorStreamState aStreamState[VBSVGA3D_MAX_VIDEO_STREAMS];
    uint32 pad[1719];
} VBSVGACOTableDXVideoProcessorEntry;
AssertCompile(sizeof(VBSVGACOTableDXVideoProcessorEntry) == 4096 * 4);

typedef struct VBSVGA3dCmdDXDefineVideoProcessor {
    VBSVGA3dVideoProcessorId videoProcessorId;

    VBSVGA3dVideoProcessorDesc desc;
} VBSVGA3dCmdDXDefineVideoProcessor;
/* VBSVGA_3D_CMD_DX_DEFINE_VIDEO_PROCESSOR */

typedef struct {
    uint32 data1;
    uint16 data2;
    uint16 data3;
    uint8 data4[8];
} VBSVGA3dGuid;

typedef struct {
    VBSVGA3dGuid DecodeProfile;
    VBSVGA3dVDOVDimension ViewDimension;
    union {
        struct {
           uint32 ArraySlice;
        } Texture2D;
        uint32 pad[4];
    };
} VBSVGA3dVDOVDesc;

typedef struct {
    SVGA3dSurfaceId sid;
    VBSVGA3dVDOVDesc desc;
    uint32 pad[6];
} VBSVGACOTableDXVideoDecoderOutputViewEntry;
AssertCompile(sizeof(VBSVGACOTableDXVideoDecoderOutputViewEntry) == 16 * 4);

typedef struct VBSVGA3dCmdDXDefineVideoDecoderOutputView {
    VBSVGA3dVideoDecoderOutputViewId videoDecoderOutputViewId;

    SVGA3dSurfaceId sid;

    VBSVGA3dVDOVDesc desc;
} VBSVGA3dCmdDXDefineVideoDecoderOutputView;
/* VBSVGA_3D_CMD_DX_DEFINE_VIDEO_DECODER_OUTPUT_VIEW */

typedef struct {
    VBSVGA3dGuid DecodeProfile;
    uint32 SampleWidth;
    uint32 SampleHeight;
    SVGA3dSurfaceFormat OutputFormat;
} VBSVGA3dVideoDecoderDesc;

typedef struct {
    VBSVGA3dGuid guidConfigBitstreamEncryption;
    VBSVGA3dGuid guidConfigMBcontrolEncryption;
    VBSVGA3dGuid guidConfigResidDiffEncryption;
    uint32 ConfigBitstreamRaw;
    uint32 ConfigMBcontrolRasterOrder;
    uint32 ConfigResidDiffHost;
    uint32 ConfigSpatialResid8;
    uint32 ConfigResid8Subtraction;
    uint32 ConfigSpatialHost8or9Clipping;
    uint32 ConfigSpatialResidInterleaved;
    uint32 ConfigIntraResidUnsigned;
    uint32 ConfigResidDiffAccelerator;
    uint32 ConfigHostInverseScan;
    uint32 ConfigSpecificIDCT;
    uint32 Config4GroupedCoefs;
    uint16 ConfigMinRenderTargetBuffCount;
    uint16 ConfigDecoderSpecific;
} VBSVGA3dVideoDecoderConfig;

typedef struct {
    VBSVGA3dVideoDecoderDesc desc;
    VBSVGA3dVideoDecoderConfig config;
    VBSVGA3dVideoDecoderOutputViewId vdovId;
    uint32 pad[31];
} VBSVGACOTableDXVideoDecoderEntry;
AssertCompile(sizeof(VBSVGACOTableDXVideoDecoderEntry) == 64 * 4);

typedef struct VBSVGA3dCmdDXDefineVideoDecoder {
    VBSVGA3dVideoDecoderId videoDecoderId;

    VBSVGA3dVideoDecoderDesc desc;
    VBSVGA3dVideoDecoderConfig config;
} VBSVGA3dCmdDXDefineVideoDecoder;
/* VBSVGA_3D_CMD_DX_DEFINE_VIDEO_DECODER */

typedef struct VBSVGA3dCmdDXVideoDecoderBeginFrame {
    VBSVGA3dVideoDecoderId videoDecoderId;
    VBSVGA3dVideoDecoderOutputViewId videoDecoderOutputViewId;
} VBSVGA3dCmdDXVideoDecoderBeginFrame;
/* VBSVGA_3D_CMD_DX_VIDEO_DECODER_BEGIN_FRAME */

typedef struct {
    SVGA3dSurfaceId sidBuffer;
    VBSVGA3dVideoDecoderBufferType bufferType;
    uint32 dataOffset;
    uint32 dataSize;
    uint32 firstMBaddress;
    uint32 numMBsInBuffer;
    /** @todo pIV, IVSize, PartialEncryption, EncryptedBlockInfo */
} VBSVGA3dVideoDecoderBufferDesc;

typedef struct VBSVGA3dCmdDXVideoDecoderSubmitBuffers {
    VBSVGA3dVideoDecoderId videoDecoderId;
    /* VBSVGA3dVideoDecoderBufferDesc[] */
} VBSVGA3dCmdDXVideoDecoderSubmitBuffers;
/* VBSVGA_3D_CMD_DX_VIDEO_DECODER_SUBMIT_BUFFERS */

typedef struct VBSVGA3dCmdDXVideoDecoderEndFrame {
    VBSVGA3dVideoDecoderId videoDecoderId;
} VBSVGA3dCmdDXVideoDecoderEndFrame;
/* VBSVGA_3D_CMD_DX_VIDEO_DECODER_END_FRAME */

typedef struct {
    uint32 FourCC;
    VBSVGA3dVPIVDimension ViewDimension;
    union {
        struct {
            uint32 MipSlice;
            uint32 ArraySlice;
        } Texture2D;
        uint32 pad[4];
    };
} VBSVGA3dVPIVDesc;

typedef struct {
    SVGA3dSurfaceId sid;
    VBSVGA3dVideoProcessorDesc contentDesc;
    VBSVGA3dVPIVDesc desc;
    uint32 pad[15];
} VBSVGACOTableDXVideoProcessorInputViewEntry;
AssertCompile(sizeof(VBSVGACOTableDXVideoProcessorInputViewEntry) == 32 * 4);

typedef struct VBSVGA3dCmdDXDefineVideoProcessorInputView {
    VBSVGA3dVideoProcessorInputViewId videoProcessorInputViewId;

    SVGA3dSurfaceId sid;

    VBSVGA3dVideoProcessorDesc contentDesc;
    VBSVGA3dVPIVDesc desc;
} VBSVGA3dCmdDXDefineVideoProcessorInputView;
/* VBSVGA_3D_CMD_DX_DEFINE_VIDEO_PROCESSOR_INPUT_VIEW */

typedef struct {
    VBSVGA3dVPOVDimension ViewDimension;
    union {
        struct {
            uint32 MipSlice;
        } Texture2D;
        struct {
            uint32 MipSlice;
            uint32 FirstArraySlice;
            uint32 ArraySize;
        } Texture2DArray;
        uint32 pad[4];
    };
} VBSVGA3dVPOVDesc;

typedef struct {
    SVGA3dSurfaceId sid;
    VBSVGA3dVideoProcessorDesc contentDesc;
    VBSVGA3dVPOVDesc desc;
    uint32 pad[15];
} VBSVGACOTableDXVideoProcessorOutputViewEntry;
AssertCompile(sizeof(VBSVGACOTableDXVideoProcessorInputViewEntry) == 32 * 4);

typedef struct VBSVGA3dCmdDXDefineVideoProcessorOutputView {
    VBSVGA3dVideoProcessorOutputViewId videoProcessorOutputViewId;

    SVGA3dSurfaceId sid;

    VBSVGA3dVideoProcessorDesc contentDesc;
    VBSVGA3dVPOVDesc desc;
} VBSVGA3dCmdDXDefineVideoProcessorOutputView;
/* VBSVGA_3D_CMD_DX_DEFINE_VIDEO_PROCESSOR_OUTPUT_VIEW */

typedef struct
{
    uint8 Enable;
    uint8 StereoFormatSeparate;
    uint16 pad;
    uint32 OutputIndex;
    uint32 InputFrameOrField;
    uint32 PastFrames;
    uint32 FutureFrames;
    /* VBSVGA3dVideoProcessorInputViewId[PastFrames + 1 + FutureFrames]:
     * [PastFrames]
     * InputSurface
     * [FutureFrames]
     *
     * If StereoFormatSeparate is 1 then more 'PastFrames + 1 + FutureFrames' ids follow:
     * [PastFramesRight]
     * InputSurfaceRight
     * [FutureFramesRight]
     */
} VBSVGA3dVideoProcessorStream;

typedef struct VBSVGA3dCmdDXVideoProcessorBlt {
    VBSVGA3dVideoProcessorId videoProcessorId;
    VBSVGA3dVideoProcessorOutputViewId videoProcessorOutputViewId;

    uint32 outputFrame;
    uint32 streamCount;
    /* VBSVGA3dVideoProcessorStream data follow. */
} VBSVGA3dCmdDXVideoProcessorBlt;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_BLT */

typedef struct VBSVGA3dCmdDXDestroyVideoDecoder {
    VBSVGA3dVideoDecoderId videoDecoderId;
} VBSVGA3dCmdDXDestroyVideoDecoder;
/* VBSVGA_3D_CMD_DX_DESTROY_VIDEO_DECODER */

typedef struct VBSVGA3dCmdDXDestroyVideoDecoderOutputView {
    VBSVGA3dVideoDecoderOutputViewId videoDecoderOutputViewId;
} VBSVGA3dCmdDXDestroyVideoDecoderOutputView;
/* VBSVGA_3D_CMD_DX_DESTROY_VIDEO_DECODER_OUTPUT_VIEW */

typedef struct VBSVGA3dCmdDXDestroyVideoProcessor {
    VBSVGA3dVideoProcessorId videoProcessorId;
} VBSVGA3dCmdDXDestroyVideoProcessor;
/* VBSVGA_3D_CMD_DX_DESTROY_VIDEO_PROCESSOR */

typedef struct VBSVGA3dCmdDXDestroyVideoProcessorInputView {
    VBSVGA3dVideoProcessorInputViewId videoProcessorInputViewId;
} VBSVGA3dCmdDXDestroyVideoProcessorInputView;
/* VBSVGA_3D_CMD_DX_DESTROY_VIDEO_PROCESSOR_INPUT_VIEW */

typedef struct VBSVGA3dCmdDXDestroyVideoProcessorOutputView {
    VBSVGA3dVideoProcessorOutputViewId videoProcessorOutputViewId;
} VBSVGA3dCmdDXDestroyVideoProcessorOutputView;
/* VBSVGA_3D_CMD_DX_DESTROY_VIDEO_PROCESSOR_OUTPUT_VIEW */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputTargetRect {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 enable;
    SVGASignedRect outputRect;
} VBSVGA3dCmdDXVideoProcessorSetOutputTargetRect;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_TARGET_RECT */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputBackgroundColor {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 ycbcr;
    VBSVGA3dVideoColor color;
} VBSVGA3dCmdDXVideoProcessorSetOutputBackgroundColor;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_BACKGROUND_COLOR */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputColorSpace {
    VBSVGA3dVideoProcessorId videoProcessorId;
    VBSVGA3dVideoProcessorColorSpace colorSpace;
} VBSVGA3dCmdDXVideoProcessorSetOutputColorSpace;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_COLOR_SPACE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputAlphaFillMode {
    VBSVGA3dVideoProcessorId videoProcessorId;
    VBSVGA3dVideoProcessorAlphaFillMode fillMode;
    uint32 streamIndex;
} VBSVGA3dCmdDXVideoProcessorSetOutputAlphaFillMode;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_ALPHA_FILL_MODE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputConstriction {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 enabled;
    uint32 width;
    uint32 height;
} VBSVGA3dCmdDXVideoProcessorSetOutputConstriction;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_CONSTRICTION */

typedef struct VBSVGA3dCmdDXVideoProcessorSetOutputStereoMode {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 enable;
} VBSVGA3dCmdDXVideoProcessorSetOutputStereoMode;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_OUTPUT_STEREO_MODE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamFrameFormat {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    VBSVGA3dVideoFrameFormat format;
} VBSVGA3dCmdDXVideoProcessorSetStreamFrameFormat;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_FRAME_FORMAT */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamColorSpace {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    VBSVGA3dVideoProcessorColorSpace colorSpace;
} VBSVGA3dCmdDXVideoProcessorSetStreamColorSpace;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_COLOR_SPACE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamOutputRate {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    VBSVGA3dVideoProcessorOutputRate outputRate;
    uint32 repeatFrame;
    SVGA3dFraction64 customRate;
} VBSVGA3dCmdDXVideoProcessorSetStreamOutputRate;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_OUTPUT_RATE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamSourceRect {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    SVGASignedRect sourceRect;
} VBSVGA3dCmdDXVideoProcessorSetStreamSourceRect;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_SOURCE_RECT */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamDestRect {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    SVGASignedRect destRect;
} VBSVGA3dCmdDXVideoProcessorSetStreamDestRect;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_DEST_RECT */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamAlpha {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    float alpha;
} VBSVGA3dCmdDXVideoProcessorSetStreamAlpha;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_ALPHA */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamPalette {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    /* uint32 entries: B8G8R8A8 or AYUV  format. */
} VBSVGA3dCmdDXVideoProcessorSetStreamPalette;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_PALETTE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamPixelAspectRatio {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    SVGA3dFraction64 sourceRatio;
    SVGA3dFraction64 destRatio;
} VBSVGA3dCmdDXVideoProcessorSetStreamPixelAspectRatio;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_PIXEL_ASPECT_RATIO */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamLumaKey {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    float lower;
    float upper;
} VBSVGA3dCmdDXVideoProcessorSetStreamLumaKey;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_LUMA_KEY */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamStereoFormat {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    VBSVGA3dVideoProcessorStereoFormat stereoFormat;
    uint8 leftViewFrame0;
    uint8 baseViewFrame0;
    uint8 pad[2];
    VBSVGA3dVideoProcessorStereoFlipMode flipMode;
    int32 monoOffset;
} VBSVGA3dCmdDXVideoProcessorSetStreamStereoFormat;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_STEREO_FORMAT */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamAutoProcessingMode {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
} VBSVGA3dCmdDXVideoProcessorSetStreamAutoProcessingMode;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_AUTO_PROCESSING_MODE */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamFilter {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    VBSVGA3dVideoProcessorFilter filter;
    int32 level;
} VBSVGA3dCmdDXVideoProcessorSetStreamFilter;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_FILTER */

typedef struct VBSVGA3dCmdDXVideoProcessorSetStreamRotation {
    VBSVGA3dVideoProcessorId videoProcessorId;
    uint32 streamIndex;
    uint32 enable;
    VBSVGA3dVideoProcessorRotation rotation;
} VBSVGA3dCmdDXVideoProcessorSetStreamRotation;
/* VBSVGA_3D_CMD_DX_VIDEO_PROCESSOR_SET_STREAM_ROTATION */

typedef struct { /* VBSVGA3D_VIDEO_CAPABILITY_DECODE_PROFILE */
    VBSVGA3dGuid DecodeProfile;
    /* SVGA video formats. */
    uint8 fAYUV;
    uint8 fNV12;
    uint8 fYUY2;
} VBSVGA3dDecodeProfileInfo;

typedef struct { /* VBSVGA3D_VIDEO_CAPABILITY_DECODE_CONFIG */
    VBSVGA3dVideoDecoderDesc desc; /* In */
    VBSVGA3dVideoDecoderConfig aConfig[1]; /* [(cbDataOut - sizeof(desc)) / sizeof(aConfig[0])] */
} VBSVGA3dDecodeConfigInfo;

typedef struct {
    VBSVGA3dVideoProcessorDeviceCaps DeviceCaps;
    VBSVGA3dVideoProcessorFeatureCaps FeatureCaps;
    VBSVGA3dVideoProcessorFilterCaps FilterCaps;
    VBSVGA3dVideoProcessorInputFormatCaps InputFormatCaps;
    VBSVGA3dVideoProcessorAutoStreamCaps AutoStreamCaps;
    VBSVGA3dVideoProcessorStereoCaps StereoCaps;
    uint32 RateConversionCapsCount;
    uint32 MaxInputStreams;
    uint32 MaxStreamStates;
} VBSVGA3dVideoProcessorCaps;

typedef struct {
    uint32 PastFrames;
    uint32 FutureFrames;
    VBSVGA3dVideoRateConversionProcessorCaps ProcessorCaps;
    VBSVGA3dVideoRateConversionITelecineCaps ITelecineCaps;
    uint32 CustomRateCount;
} VBSVGA3dVideoProcessorRateCaps;

typedef struct {
    SVGA3dFraction64 CustomRate;
    uint32 OutputFrames;
    uint8 InputInterlaced;
    uint8 pad[3];
    uint32 InputFramesOrFields;
} VBSVGA3dVideoProcessorCustomRateCaps;

typedef struct {
    int32 Minimum;
    int32 Maximum;
    int32 Default;
    float Multiplier;
} VBSVGA3dVideoProcessorFilterRange;

typedef struct {
    /* SVGA video processor formats. */
    VBSVGA3dVideoProcessorFormatSupport fR8_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fR16_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fNV12;
    VBSVGA3dVideoProcessorFormatSupport fYUY2;
    VBSVGA3dVideoProcessorFormatSupport fR16G16B16A16_FLOAT;
    VBSVGA3dVideoProcessorFormatSupport fB8G8R8X8_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fB8G8R8A8_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fR8G8B8A8_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fR10G10B10A2_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fR10G10B10_XR_BIAS_A2_UNORM;
    VBSVGA3dVideoProcessorFormatSupport fR8G8B8A8_UNORM_SRGB;
    VBSVGA3dVideoProcessorFormatSupport fB8G8R8A8_UNORM_SRGB;

    VBSVGA3dVideoProcessorCaps Caps;
    VBSVGA3dVideoProcessorRateCaps RateCaps;
    VBSVGA3dVideoProcessorCustomRateCaps aCustomRateCaps[VBSVGA3D_MAX_VIDEO_CUSTOM_RATE_CAPS];
    VBSVGA3dVideoProcessorFilterRange aFilterRange[VBSVGA3D_VP_MAX_FILTER_COUNT];
} VBSVGA3dVideoProcessorEnumInfo;

typedef struct { /* VBSVGA3D_VIDEO_CAPABILITY_PROCESSOR_ENUM */
    VBSVGA3dVideoProcessorDesc desc; /* In */
    VBSVGA3dVideoProcessorEnumInfo info;
} VBSVGA3dProcessorEnumInfo;

/* Layout of memory object for VBSVGA3dCmdDXGetVideoCapability command. */
typedef struct {
    uint64 fenceValue; /* Host sets this to VBSVGA3dCmdDXGetVideoCapability::fenceValue after updating the data. */
    uint32 cbDataOut; /* Size in bytes of data written by host excluding u64Fence and cbDataOut fields. */
    union
    {
        VBSVGA3dDecodeProfileInfo aDecodeProfile[1]; /* [cbDataOut / sizeof(aDecodeProfile[0])] */
        VBSVGA3dDecodeConfigInfo config;
        VBSVGA3dProcessorEnumInfo processorEnum;
    } data;
} VBSVGA3dVideoCapabilityMobLayout;

typedef struct VBSVGA3dCmdDXGetVideoCapability {
    VBSVGA3dVideoCapability capability;
    uint32 mobid;
    uint32 offsetInBytes;
    uint32 sizeInBytes;
    uint64 fenceValue;
} VBSVGA3dCmdDXGetVideoCapability;
/* VBSVGA_3D_CMD_DX_GET_VIDEO_CAPABILITY */

typedef struct VBSVGA3dCmdDXClearView
{
   uint32 viewId;
   SVGA3dRGBAFloat color;
   /* followed by SVGASignedRect array */
} VBSVGA3dCmdDXClearView;
/* VBSVGA_3D_CMD_DX_CLEAR_RTV
 * VBSVGA_3D_CMD_DX_CLEAR_UAV
 * VBSVGA_3D_CMD_DX_CLEAR_VDOV
 * VBSVGA_3D_CMD_DX_CLEAR_VPIV
 * VBSVGA_3D_CMD_DX_CLEAR_VPOV
 */

#endif /* !VBOX_INCLUDED_SRC_Graphics_vmsvga_include_vbsvga3d_dx_h */
