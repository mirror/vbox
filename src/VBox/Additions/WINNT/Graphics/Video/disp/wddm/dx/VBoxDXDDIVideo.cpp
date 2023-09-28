/* $Id$ */
/** @file
 * VirtualBox D3D11 user mode DDI interface for video.
 */

/*
 * Copyright (C) 2020-2023 Oracle and/or its affiliates.
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

#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#define INITGUID

#include <iprt/alloc.h>
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <VBox/log.h>

#include <iprt/win/windows.h>
#include <iprt/win/d3dkmthk.h>

#include <d3d10umddi.h>

#include "VBoxDX.h"

#include <VBoxWddmUmHlp.h>

static const GUID gaDecoderProfiles[] =
{
    //D3D11_DECODER_PROFILE_MPEG2_MOCOMP,
    //D3D11_DECODER_PROFILE_MPEG2_IDCT,
    //D3D11_DECODER_PROFILE_MPEG2_VLD,
    //D3D11_DECODER_PROFILE_MPEG1_VLD,
    //D3D11_DECODER_PROFILE_MPEG2and1_VLD,
    //D3D11_DECODER_PROFILE_H264_MOCOMP_NOFGT,
    //D3D11_DECODER_PROFILE_H264_MOCOMP_FGT,
    //D3D11_DECODER_PROFILE_H264_IDCT_NOFGT,
    //D3D11_DECODER_PROFILE_H264_IDCT_FGT,
    D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
    //D3D11_DECODER_PROFILE_H264_VLD_FGT,
    //D3D11_DECODER_PROFILE_H264_VLD_WITHFMOASO_NOFGT,
    //D3D11_DECODER_PROFILE_H264_VLD_STEREO_PROGRESSIVE_NOFGT,
    //D3D11_DECODER_PROFILE_H264_VLD_STEREO_NOFGT,
    //D3D11_DECODER_PROFILE_H264_VLD_MULTIVIEW_NOFGT,
    //D3D11_DECODER_PROFILE_WMV8_POSTPROC,
    //D3D11_DECODER_PROFILE_WMV8_MOCOMP,
    //D3D11_DECODER_PROFILE_WMV9_POSTPROC,
    //D3D11_DECODER_PROFILE_WMV9_MOCOMP,
    //D3D11_DECODER_PROFILE_WMV9_IDCT,
    //D3D11_DECODER_PROFILE_VC1_POSTPROC,
    //D3D11_DECODER_PROFILE_VC1_MOCOMP,
    //D3D11_DECODER_PROFILE_VC1_IDCT,
    //D3D11_DECODER_PROFILE_VC1_VLD,
    //D3D11_DECODER_PROFILE_VC1_D2010,
    //D3D11_DECODER_PROFILE_MPEG4PT2_VLD_SIMPLE,
    //D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_NOGMC,
    //D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_GMC,
    //D3D11_DECODER_PROFILE_HEVC_VLD_MAIN,
    //D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10,
    //D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
    //D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2,
    //D3D11_DECODER_PROFILE_VP8_VLD,
    //D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0,
    //D3D11_DECODER_PROFILE_AV1_VLD_PROFILE1,
    //D3D11_DECODER_PROFILE_AV1_VLD_PROFILE2,
    //D3D11_DECODER_PROFILE_AV1_VLD_12BIT_PROFILE2,
    //D3D11_DECODER_PROFILE_AV1_VLD_12BIT_PROFILE2_420,
};


typedef struct VBOXDXVIDEOPROCESSORINPUTVIEW
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTVIDEOPROCESSORINPUTVIEW hRTView;
    D3D11_1DDIARG_CREATEVIDEOPROCESSORINPUTVIEW CreateData;
} VBOXDXVIDEOPROCESSORINPUTVIEW, *PVBOXDXVIDEOPROCESSORINPUTVIEW;

typedef struct VBOXDXVIDEOPROCESSOROUTPUTVIEW
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTVIDEOPROCESSOROUTPUTVIEW hRTView;
    D3D11_1DDIARG_CREATEVIDEOPROCESSOROUTPUTVIEW CreateData;
} VBOXDXVIDEOPROCESSOROUTPUTVIEW, *PVBOXDXVIDEOPROCESSOROUTPUTVIEW;

typedef struct VBOXDXVIDEOPROCESSORFILTER
{
    BOOL Enable;
    int Level;
} VBOXDXVIDEOPROCESSORFILTER;

typedef struct VBOXDXVIDEOPROCESSORSTREAM
{
    D3D11_1DDI_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode;
    D3D11_1DDI_VIDEO_FRAME_FORMAT FrameFormat;
    D3D11_1DDI_VIDEO_PROCESSOR_COLOR_SPACE ColorSpace;
    struct {
        D3D11_1DDI_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate;
        BOOL RepeatFrame;
        DXGI_RATIONAL CustomRate;
    } OutputRate;
    struct {
        BOOL Enable;
        RECT SourceRect;
    } SourceRect;
    struct {
        BOOL Enable;
        RECT DestRect;
    } DestRect;
    struct {
        BOOL Enable;
        FLOAT Lower;
        FLOAT Upper;
    } LumaKey;
    struct {
        BOOL Enable;
    } AutoProcessingMode;
    VBOXDXVIDEOPROCESSORFILTER aFilters[D3D11_1DDI_VIDEO_PROCESSOR_FILTER_STEREO_ADJUSTMENT + 1];
} VBOXDXVIDEOPROCESSORSTREAM;

typedef struct VBOXDXVIDEOPROCESSOR
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTVIDEOPROCESSOR hRTVideoProcessor;
    uint32_t au32Dummy[1];
    struct {
        BOOL Enable;
        RECT Rect;
    } OutputRect;
    struct {
        BOOL YCbCr;
        D3D11_1DDI_VIDEO_COLOR Color;
    } OutputBackgroundColor;
    D3D11_1DDI_VIDEO_PROCESSOR_COLOR_SPACE Colorspace;
    struct {
        BOOL Enabled;
        SIZE ConstrictonSize;
    } OutputConstriction;

    VBOXDXVIDEOPROCESSORSTREAM aStreams[2];
} VBOXDXVIDEOPROCESSOR, *PVBOXDXVIDEOPROCESSOR;

typedef struct VBOXDXVIDEOPROCESSORENUM
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTVIDEOPROCESSORENUM hRTVideoProcessorEnum;
    D3D11_1DDI_VIDEO_PROCESSOR_CONTENT_DESC Desc;
} VBOXDXVIDEOPROCESSORENUM, *PVBOXDXVIDEOPROCESSORENUM;


typedef struct VBOXDXVIDEODECODEROUTPUTVIEW
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTVIDEODECODEROUTPUTVIEW hRTView;
    D3D11_1DDIARG_CREATEVIDEODECODEROUTPUTVIEW CreateData;
} VBOXDXVIDEODECODEROUTPUTVIEW, *PVBOXDXVIDEODECODEROUTPUTVIEW;

typedef struct VBOXDXVIDEODECODER
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTDECODE hRTVideoDecoder;
    D3D11_1DDIARG_CREATEVIDEODECODER CreateData;
    struct {
        PVBOXDXVIDEODECODEROUTPUTVIEW pOutputView;
    } Frame;
} VBOXDXVIDEODECODER, *PVBOXDXVIDEODECODER;

typedef struct VBOXDXVIDEOCRYPTOSESSION
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTCRYPTOSESSION hRTCryptoSession;
    uint32_t au32Dummy[3];
} VBOXDXVIDEOCRYPTOSESSION, *PVBOXDXVIDEOCRYPTOSESSION;

typedef struct VBOXDXVIDEOAUTHCHANNEL
{
    PVBOXDX_DEVICE pDevice;
    D3D11_1DDI_HRTAUTHCHANNEL hRTAuthChannel;
    uint32_t au32Dummy[4];
} VBOXDXVIDEOAUTHCHANNEL, *PVBOXDXVIDEOAUTHCHANNEL;


static VOID APIENTRY ddi11_1GetVideoDecoderProfileCount(
    D3D10DDI_HDEVICE hDevice,
    UINT *pDecodeProfileCount)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    *pDecodeProfileCount = RT_ELEMENTS(gaDecoderProfiles);
    return;
}

static VOID APIENTRY ddi11_1GetVideoDecoderProfile(
    D3D10DDI_HDEVICE hDevice,
    UINT Index,
    GUID *pGuid)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    D3D11_1DDI_VIDEO_DECODER_DESC *pDecodeDesc = (D3D11_1DDI_VIDEO_DECODER_DESC *)pGuid;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, Index, pDecodeDesc);
    *pGuid = gaDecoderProfiles[Index];
    return;
}

static VOID APIENTRY ddi11_1CheckVideoDecoderFormat(
    D3D10DDI_HDEVICE hDevice,
    CONST GUID *pDecoderProfile,
    DXGI_FORMAT Format,
    BOOL *pSupported)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecoderProfile, Format);
    if (   Format == DXGI_FORMAT_NV12
        || Format == DXGI_FORMAT_B8G8R8A8_UNORM)
        *pSupported = TRUE;
    else
        *pSupported = FALSE;
    return;
}

static VOID APIENTRY ddi11_1GetVideoDecoderConfigCount(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_VIDEO_DECODER_DESC *pDecodeDesc,
    UINT *pConfigCount)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecodeDesc);
    *pConfigCount = 1;
    return;
}

static VOID APIENTRY ddi11_1GetVideoDecoderConfig(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_VIDEO_DECODER_DESC *pDecodeDesc,
    UINT Index,
    D3D11_1DDI_VIDEO_DECODER_CONFIG *pConfig)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecodeDesc, Index);
    pConfig->guidConfigBitstreamEncryption = DXVADDI_NoEncrypt;
    pConfig->guidConfigMBcontrolEncryption = DXVADDI_NoEncrypt;
    pConfig->guidConfigResidDiffEncryption = DXVADDI_NoEncrypt;
    pConfig->ConfigBitstreamRaw = 1;
    pConfig->ConfigMBcontrolRasterOrder = 0;
    pConfig->ConfigResidDiffHost = 0;
    pConfig->ConfigSpatialResid8 = 0;
    pConfig->ConfigResid8Subtraction = 0;
    pConfig->ConfigSpatialHost8or9Clipping = 0;
    pConfig->ConfigSpatialResidInterleaved = 0;
    pConfig->ConfigIntraResidUnsigned = 0;
    pConfig->ConfigResidDiffAccelerator = 0;
    pConfig->ConfigHostInverseScan = 0;
    pConfig->ConfigSpecificIDCT = 0;
    pConfig->Config4GroupedCoefs = 0;
    pConfig->ConfigMinRenderTargetBuffCount = 0;
    pConfig->ConfigDecoderSpecific = 0;
    return;
}

/* D3D11_1DDI_VIDEO_DECODER_BUFFER_INFO::Usage has to be D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY,
 * otherwise Windows refuses to use the decoder.
 */
static D3D11_1DDI_VIDEO_DECODER_BUFFER_INFO const aBufferInfo[] =
{
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_MACROBLOCK_CONTROL, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_RESIDUAL_DIFFERENCE, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_DEBLOCKING_CONTROL, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_SLICE_CONTROL, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_BITSTREAM, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_MOTION_VECTOR, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
    { D3D11_1DDI_VIDEO_DECODER_BUFFER_FILM_GRAIN, _64K, D3D11_1DDI_VIDEO_USAGE_OPTIMAL_QUALITY },
};

static VOID APIENTRY ddi11_1GetVideoDecoderBufferTypeCount(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_VIDEO_DECODER_DESC *pDecodeDesc,
    UINT *pBufferTypeCount)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecodeDesc);
    *pBufferTypeCount = RT_ELEMENTS(aBufferInfo);
    return;
}

static VOID APIENTRY ddi11_1GetVideoDecoderBufferInfo(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_VIDEO_DECODER_DESC *pDecodeDesc,
    UINT Index,
    D3D11_1DDI_VIDEO_DECODER_BUFFER_INFO *pInfo)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecodeDesc);
    *pInfo = aBufferInfo[Index];
    return;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoDecoderSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEODECODER *pDecoder)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pDecoder);
    return sizeof(VBOXDXVIDEODECODER);
}

static HRESULT APIENTRY ddi11_1CreateVideoDecoder(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEODECODER *pCreateData,
    D3D11_1DDI_HDECODE hDecoder,
    D3D11_1DDI_HRTDECODE hRTDecoder)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODER pVideoDecoder = (PVBOXDXVIDEODECODER)hDecoder.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    RT_ZERO(*pVideoDecoder);
    pVideoDecoder->pDevice = pDevice;
    pVideoDecoder->hRTVideoDecoder = hRTDecoder;
    pVideoDecoder->CreateData = *pCreateData;
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoDecoder(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODER pVideoDecoder = (PVBOXDXVIDEODECODER)hDecoder.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoDecoder);
    return;
}

static HRESULT APIENTRY ddi11_1VideoDecoderExtension(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder,
    CONST D3D11_1DDIARG_VIDEODECODEREXTENSION *pExtension)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hDecoder, pExtension);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1VideoDecoderBeginFrame(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder,
    CONST D3D11_1DDIARG_VIDEODECODERBEGINFRAME *pBeginFrame)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODER pVideoDecoder = (PVBOXDXVIDEODECODER)hDecoder.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoDecoder->Frame.pOutputView = (PVBOXDXVIDEODECODEROUTPUTVIEW)pBeginFrame->hOutputView.pDrvPrivate;
    //pBeginFrame->pContentKey;
    //pBeginFrame->ContentKeySize;
    return S_OK;
}

static VOID APIENTRY ddi11_1VideoDecoderEndFrame(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODER pVideoDecoder = (PVBOXDXVIDEODECODER)hDecoder.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoDecoder->Frame.pOutputView = NULL;
    return;
}

static HRESULT APIENTRY ddi11_1VideoDecoderSubmitBuffers(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder,
    UINT BufferCount,
    CONST D3D11_1DDI_VIDEO_DECODER_BUFFER_DESC *pBufferDesc)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODER pVideoDecoder = (PVBOXDXVIDEODECODER)hDecoder.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoDecoder);
    for (UINT i = 0; i < BufferCount; ++i)
    {
        D3D11_1DDI_VIDEO_DECODER_BUFFER_DESC const *pDesc = &pBufferDesc[i];
        LogFlowFunc(("at %d, size %d\n", pDesc->DataOffset, pDesc->DataSize));
        RT_NOREF(pDesc);
    }
    return S_OK;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoProcessorEnumSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSORENUM *pProcessorEnum)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    //DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pProcessorEnum);
    return sizeof(VBOXDXVIDEOPROCESSORENUM);
}

static HRESULT APIENTRY ddi11_1CreateVideoProcessorEnum(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSORENUM *pCreateData,
    D3D11_1DDI_HVIDEOPROCESSORENUM hVideoProcessorEnum,
    D3D11_1DDI_HRTVIDEOPROCESSORENUM hRTVideoProcessorEnum)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSORENUM pVideoProcessorEnum = (PVBOXDXVIDEOPROCESSORENUM)hVideoProcessorEnum.pDrvPrivate;
    //DEBUG_BREAKPOINT_TEST();
    RT_ZERO(*pVideoProcessorEnum);
    pVideoProcessorEnum->pDevice = pDevice;
    pVideoProcessorEnum->hRTVideoProcessorEnum = hRTVideoProcessorEnum;
    pVideoProcessorEnum->Desc = pCreateData->Desc;
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoProcessorEnum(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hProcessorEnum)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSORENUM pVideoProcessorEnum = (PVBOXDXVIDEOPROCESSORENUM)hProcessorEnum.pDrvPrivate;
    //DEBUG_BREAKPOINT_TEST();
    RT_ZERO(*pVideoProcessorEnum);
    RT_NOREF(pDevice);
    return;
}

static VOID APIENTRY ddi11_1CheckVideoProcessorFormat(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hVideoProcessorEnum,
    DXGI_FORMAT Format,
    UINT *pSupported)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessorEnum, Format);
    if (   Format == DXGI_FORMAT_NV12
        || Format == DXGI_FORMAT_B8G8R8A8_UNORM)
        *pSupported = D3D11_1DDI_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT
                    | D3D11_1DDI_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT;
    else
        *pSupported = 0;
    return;
}

static VOID APIENTRY ddi11_1GetVideoProcessorCaps(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hProcessorEnum,
    D3D11_1DDI_VIDEO_PROCESSOR_CAPS *pCaps)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSORENUM pVideoProcessorEnum = (PVBOXDXVIDEOPROCESSORENUM)hProcessorEnum.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoProcessorEnum);
    RT_ZERO(*pCaps);
    pCaps->DeviceCaps = D3D11_1DDI_VIDEO_PROCESSOR_DEVICE_CAPS_LINEAR_SPACE
                      | D3D11_1DDI_VIDEO_PROCESSOR_DEVICE_CAPS_xvYCC
                      | D3D11_1DDI_VIDEO_PROCESSOR_DEVICE_CAPS_RGB_RANGE_CONVERSION
                      | D3D11_1DDI_VIDEO_PROCESSOR_DEVICE_CAPS_YCbCr_MATRIX_CONVERSION
                      | D3D11_1DDI_VIDEO_PROCESSOR_DEVICE_CAPS_NOMINAL_RANGE;
    pCaps->FeatureCaps = D3D11_1DDI_VIDEO_PROCESSOR_FEATURE_CAPS_ALPHA_FILL
                       | D3D11_1DDI_VIDEO_PROCESSOR_FEATURE_CAPS_CONSTRICTION
                       | D3D11_1DDI_VIDEO_PROCESSOR_FEATURE_CAPS_LUMA_KEY;
    pCaps->FilterCaps = D3D11_1DDI_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS
                      | D3D11_1DDI_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST
                      | D3D11_1DDI_VIDEO_PROCESSOR_FILTER_CAPS_HUE
                      | D3D11_1DDI_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION;
    pCaps->InputFormatCaps = D3D11_1DDI_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_INTERLACED
                           | D3D11_1DDI_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_PROCAMP
                           | D3D11_1DDI_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_LUMA_KEY;
    pCaps->AutoStreamCaps = 0;
    pCaps->StereoCaps = 0;
    pCaps->RateConversionCapsCount = 1;
    pCaps->MaxInputStreams = 2;
    pCaps->MaxStreamStates = 2;
    return;
}

static VOID APIENTRY ddi11_1GetVideoProcessorRateConversionCaps(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hProcessorEnum,
    UINT RateConversionIndex,
    D3D11_1DDI_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hProcessorEnum, RateConversionIndex);
    RT_ZERO(*pCaps);
    pCaps->PastFrames = 2;
    pCaps->FutureFrames = 2;
    pCaps->ConversionCaps = D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_DEINTERLACE_BLEND
                          | D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_DEINTERLACE_BOB
                          | D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_DEINTERLACE_ADAPTIVE
                          | D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_DEINTERLACE_MOTION_COMPENSATION
                          | D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_INVERSE_TELECINE
                          | D3D11_1DDI_VIDEO_PROCESSOR_CONVERSION_CAPS_FRAME_RATE_CONVERSION;
    pCaps->ITelecineCaps = D3D11_1DDI_VIDEO_PROCESSOR_ITELECINE_CAPS_32
                         | D3D11_1DDI_VIDEO_PROCESSOR_ITELECINE_CAPS_22
                         | D3D11_1DDI_VIDEO_PROCESSOR_ITELECINE_CAPS_2224
                         | D3D11_1DDI_VIDEO_PROCESSOR_ITELECINE_CAPS_2332;
    pCaps->CustomRateCount  = 0;
    return;
}

static VOID APIENTRY ddi11_1GetVideoProcessorCustomRate(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hProcessorEnum,
    UINT CustomRateIndex,
    UINT RateConversionIndex,
    D3D11_1DDI_VIDEO_PROCESSOR_CUSTOM_RATE *pRate)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hProcessorEnum, CustomRateIndex, RateConversionIndex, pRate);
    return;
}

static VOID APIENTRY ddi11_1GetVideoProcessorFilterRange(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORENUM hProcessorEnum,
    D3D11_1DDI_VIDEO_PROCESSOR_FILTER Filter,
    D3D11_1DDI_VIDEO_PROCESSOR_FILTER_RANGE *pFilterRange)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hProcessorEnum, Filter);
    pFilterRange->Minimum = 0;
    pFilterRange->Maximum = 100;
    pFilterRange->Default = 50;
    pFilterRange->Multiplier = 0.01f;
    return;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoProcessorSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSOR *pVideoProcessor)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoProcessor);
    return sizeof(VBOXDXVIDEOPROCESSOR);
}

static HRESULT APIENTRY ddi11_1CreateVideoProcessor(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSOR *pCreateData,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    D3D11_1DDI_HRTVIDEOPROCESSOR hRTVideoProcessor)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_ZERO(*pVideoProcessor);
    pVideoProcessor->pDevice = pDevice;
    pVideoProcessor->hRTVideoProcessor = hRTVideoProcessor;
    RT_NOREF(pCreateData);
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoProcessor(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoProcessor);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputTargetRect(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    BOOL Enable,
    CONST RECT *pOutputRect)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->OutputRect.Enable = Enable;
    pVideoProcessor->OutputRect.Rect = *pOutputRect;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputBackgroundColor(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    BOOL YCbCr,
    CONST D3D11_1DDI_VIDEO_COLOR *pColor)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->OutputBackgroundColor.YCbCr = YCbCr;
    pVideoProcessor->OutputBackgroundColor.Color = *pColor;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputColorSpace(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    CONST D3D11_1DDI_VIDEO_PROCESSOR_COLOR_SPACE *pColorspace)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->Colorspace = *pColorspace;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputAlphaFillMode(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    D3D11_1DDI_VIDEO_PROCESSOR_ALPHA_FILL_MODE FillMode,
    UINT StreamIndex)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].AlphaFillMode = FillMode;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputConstriction(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    BOOL Enabled,
    SIZE ConstrictonSize)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->OutputConstriction.Enabled = Enabled;
    pVideoProcessor->OutputConstriction.ConstrictonSize = ConstrictonSize;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetOutputStereoMode(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    BOOL Enable)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, Enable);
    return;
}

static HRESULT APIENTRY ddi11_1VideoProcessorSetOutputExtension(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    CONST GUID *pGuid,
    UINT DataSize,
    void *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, pGuid, DataSize, pData);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1VideoProcessorGetOutputExtension(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    CONST GUID *pGuid,
    UINT DataSize,
    void *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, pGuid, DataSize, pData);
    return S_OK;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamFrameFormat(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    D3D11_1DDI_VIDEO_FRAME_FORMAT Format)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].FrameFormat = Format;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamColorSpace(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    CONST D3D11_1DDI_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].ColorSpace = *pColorSpace;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamOutputRate(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    D3D11_1DDI_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate,
    BOOL RepeatFrame,
    CONST DXGI_RATIONAL *pCustomRate)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].OutputRate.OutputRate = OutputRate;
    pVideoProcessor->aStreams[StreamIndex].OutputRate.RepeatFrame = RepeatFrame;
    pVideoProcessor->aStreams[StreamIndex].OutputRate.CustomRate = *pCustomRate;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamSourceRect(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    CONST RECT *pSourceRect)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].SourceRect.Enable = Enable;
    pVideoProcessor->aStreams[StreamIndex].SourceRect.SourceRect = *pSourceRect;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamDestRect(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    CONST RECT *pDestRect)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].DestRect.Enable = Enable;
    pVideoProcessor->aStreams[StreamIndex].DestRect.DestRect = *pDestRect;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamAlpha(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    FLOAT Alpha)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, Enable, Alpha);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamPalette(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    UINT Count,
    CONST UINT *pEntries)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, Count, pEntries);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamPixelAspectRatio(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    CONST DXGI_RATIONAL *pSourceRatio,
    CONST DXGI_RATIONAL *pDestRatio)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, Enable, pSourceRatio, pDestRatio);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamLumaKey(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    FLOAT Lower,
    FLOAT Upper)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].LumaKey.Enable = Enable;
    pVideoProcessor->aStreams[StreamIndex].LumaKey.Lower = Lower;
    pVideoProcessor->aStreams[StreamIndex].LumaKey.Upper = Upper;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamStereoFormat(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    D3D11_1DDI_VIDEO_PROCESSOR_STEREO_FORMAT StereoFormat,
    BOOL LeftViewFrame0,
    BOOL BaseViewFrame0,
    D3D11_1DDI_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
    int MonoOffset)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, Enable, StereoFormat, LeftViewFrame0, BaseViewFrame0, FlipMode, MonoOffset);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamAutoProcessingMode(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].AutoProcessingMode.Enable = Enable;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamFilter(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    D3D11_1DDI_VIDEO_PROCESSOR_FILTER Filter,
    BOOL Enable,
    int Level)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOR pVideoProcessor = (PVBOXDXVIDEOPROCESSOR)hVideoProcessor.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessor->aStreams[StreamIndex].aFilters[Filter].Enable = Enable;
    pVideoProcessor->aStreams[StreamIndex].aFilters[Filter].Level = Level;
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorSetStreamRotation(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    BOOL Enable,
    D3D11_1DDI_VIDEO_PROCESSOR_ROTATION Rotation)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, Enable, Rotation);
    return;
}

static HRESULT APIENTRY ddi11_1VideoProcessorSetStreamExtension(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    CONST GUID *pGuid,
    UINT DataSize,
    void *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, pGuid, DataSize, pData);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1VideoProcessorGetStreamExtension(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    UINT StreamIndex,
    CONST GUID *pGuid,
    UINT DataSize,
    void *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, StreamIndex, pGuid, DataSize, pData);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1VideoProcessorBlt(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOR hVideoProcessor,
    D3D11_1DDI_HVIDEOPROCESSOROUTPUTVIEW hOutputView,
    UINT OutputFrame,
    UINT StreamCount,
    CONST D3D11_1DDI_VIDEO_PROCESSOR_STREAM *pStream)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hVideoProcessor, hOutputView, OutputFrame, StreamCount, pStream);
    return S_OK;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoDecoderOutputViewSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEODECODEROUTPUTVIEW *pView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pView);
    return sizeof(VBOXDXVIDEODECODEROUTPUTVIEW);
}

static HRESULT APIENTRY ddi11_1CreateVideoDecoderOutputView(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEODECODEROUTPUTVIEW *pCreateData,
    D3D11_1DDI_HVIDEODECODEROUTPUTVIEW hView,
    D3D11_1DDI_HRTVIDEODECODEROUTPUTVIEW hRTView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODEROUTPUTVIEW pVideoDecoderOutputView = (PVBOXDXVIDEODECODEROUTPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoDecoderOutputView->pDevice = pDevice;
    pVideoDecoderOutputView->hRTView = hRTView;
    pVideoDecoderOutputView->CreateData = *pCreateData;
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoDecoderOutputView(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEODECODEROUTPUTVIEW hView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEODECODEROUTPUTVIEW pVideoDecoderOutputView = (PVBOXDXVIDEODECODEROUTPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoDecoderOutputView);
    return;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoProcessorInputViewSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSORINPUTVIEW *pView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pView);
    return sizeof(VBOXDXVIDEOPROCESSORINPUTVIEW);
}

static HRESULT APIENTRY ddi11_1CreateVideoProcessorInputView(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSORINPUTVIEW *pCreateData,
    D3D11_1DDI_HVIDEOPROCESSORINPUTVIEW hView,
    D3D11_1DDI_HRTVIDEOPROCESSORINPUTVIEW hRTView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSORINPUTVIEW pVideoProcessorInputView = (PVBOXDXVIDEOPROCESSORINPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessorInputView->pDevice = pDevice;
    pVideoProcessorInputView->hRTView = hRTView;
    pVideoProcessorInputView->CreateData = *pCreateData;
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoProcessorInputView(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORINPUTVIEW hView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSORINPUTVIEW pVideoProcessorInputView = (PVBOXDXVIDEOPROCESSORINPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoProcessorInputView);
    return;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateVideoProcessorOutputViewSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSOROUTPUTVIEW *pView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pView);
    return sizeof(VBOXDXVIDEOPROCESSOROUTPUTVIEW);
}

static HRESULT APIENTRY ddi11_1CreateVideoProcessorOutputView(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEVIDEOPROCESSOROUTPUTVIEW *pCreateData,
    D3D11_1DDI_HVIDEOPROCESSOROUTPUTVIEW hView,
    D3D11_1DDI_HRTVIDEOPROCESSOROUTPUTVIEW hRTView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOROUTPUTVIEW pVideoProcessorOutputView = (PVBOXDXVIDEOPROCESSOROUTPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);
    pVideoProcessorOutputView->pDevice = pDevice;
    pVideoProcessorOutputView->hRTView = hRTView;
    pVideoProcessorOutputView->CreateData = *pCreateData;
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyVideoProcessorOutputView(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSOROUTPUTVIEW hView)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOPROCESSOROUTPUTVIEW pVideoProcessorOutputView = (PVBOXDXVIDEOPROCESSOROUTPUTVIEW)hView.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pVideoProcessorOutputView);
    return;
}

static VOID APIENTRY ddi11_1VideoProcessorInputViewReadAfterWriteHazard(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HVIDEOPROCESSORINPUTVIEW hView,
    D3D10DDI_HRESOURCE hResource)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hView, hResource);
    return;
}

static HRESULT APIENTRY ddi11_1GetContentProtectionCaps(
    D3D10DDI_HDEVICE hDevice,
    CONST GUID *pCryptoType,
    CONST GUID *pDecodeProfile,
    D3D11_1DDI_VIDEO_CONTENT_PROTECTION_CAPS *pCaps)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCryptoType, pDecodeProfile, pCaps);
    return D3DERR_UNSUPPORTEDCRYPTO;
}

static HRESULT APIENTRY ddi11_1GetCryptoKeyExchangeType(
    D3D10DDI_HDEVICE hDevice,
    CONST GUID *pCryptoType,
    CONST GUID *pDecodeProfile,
    UINT Index,
    GUID *pKeyExchangeType)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCryptoType, pDecodeProfile, Index, pKeyExchangeType);
    return D3DERR_UNSUPPORTEDCRYPTO;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateCryptoSessionSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATECRYPTOSESSION *pCreateData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCreateData);
    return sizeof(VBOXDXVIDEOCRYPTOSESSION);
}

static HRESULT APIENTRY ddi11_1CreateCryptoSession(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATECRYPTOSESSION *pCreateData,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    D3D11_1DDI_HRTCRYPTOSESSION hRTCryptoSession)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOCRYPTOSESSION pCryptoSession = (PVBOXDXVIDEOCRYPTOSESSION)hCryptoSession.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_ZERO(*pCryptoSession);
    pCryptoSession->pDevice = pDevice;
    pCryptoSession->hRTCryptoSession = hRTCryptoSession;
    RT_NOREF(pCreateData);
    return D3DDDIERR_UNSUPPORTEDCRYPTO;
}

static VOID APIENTRY ddi11_1DestroyCryptoSession(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession);
    return;
}

static VOID APIENTRY ddi11_1GetCertificateSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_CERTIFICATE_INFO *pCertificateInfo,
    UINT *pCertificateSize)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCertificateInfo);
    *pCertificateSize = 0;
    return;
}

static VOID APIENTRY ddi11_1GetCertificate(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDI_CERTIFICATE_INFO *pCertificateInfo,
    UINT CertificateSize,
    BYTE *pCertificate)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCertificateInfo, CertificateSize, pCertificate);
    return;
}

static HRESULT APIENTRY ddi11_1NegotiateCryptoSessionKeyExchange(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    UINT DataSize,
    BYTE *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, DataSize ,pData);
    return S_OK;
}

static VOID APIENTRY ddi11_1EncryptionBlt(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    D3D10DDI_HRESOURCE hSrcResource,
    D3D10DDI_HRESOURCE hDstResource,
    UINT IVSize,
    CONST VOID *pIV)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, hSrcResource, hDstResource, IVSize, pIV);
    return;
}

static VOID APIENTRY ddi11_1DecryptionBlt(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    D3D10DDI_HRESOURCE hSrcResource,
    D3D10DDI_HRESOURCE hDstResource,
    CONST D3D11_1DDI_ENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
    UINT ContentKeySize,
    CONST VOID *pContentKey,
    UINT IVSize,
    CONST VOID *pIV)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, hSrcResource, hDstResource, pEncryptedBlockInfo, ContentKeySize, pContentKey, IVSize, pIV);
    return;
}

static VOID APIENTRY ddi11_1StartSessionKeyRefresh(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    UINT RandomNumberSize,
    VOID *pRandomNumber)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, RandomNumberSize, pRandomNumber);
    return;
}

static VOID APIENTRY ddi11_1FinishSessionKeyRefresh(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession);
    return;
}

static VOID APIENTRY ddi11_1GetEncryptionBltKey(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    UINT KeySize,
    VOID *pReadbackKey)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, KeySize, pReadbackKey);
    return;
}

static SIZE_T APIENTRY ddi11_1CalcPrivateAuthenticatedChannelSize(
    D3D10DDI_HDEVICE hDevice,
    CONST D3D11_1DDIARG_CREATEAUTHENTICATEDCHANNEL *pCreateData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pCreateData);
    return sizeof(VBOXDXVIDEOAUTHCHANNEL);
}

static HRESULT APIENTRY ddi11_1CreateAuthenticatedChannel(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDIARG_CREATEAUTHENTICATEDCHANNEL *pCreateData,
    D3D11_1DDI_HAUTHCHANNEL hAuthChannel,
    D3D11_1DDI_HRTAUTHCHANNEL hRTAuthChannel)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    PVBOXDXVIDEOAUTHCHANNEL pAuthChannel = (PVBOXDXVIDEOAUTHCHANNEL)hAuthChannel.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_ZERO(*pAuthChannel);
    pAuthChannel->pDevice = pDevice;
    pAuthChannel->hRTAuthChannel = hRTAuthChannel;
    RT_NOREF(pCreateData);
    return S_OK;
}

static VOID APIENTRY ddi11_1DestroyAuthenticatedChannel(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HAUTHCHANNEL hAuthChannel)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hAuthChannel);
    return;
}

static HRESULT APIENTRY ddi11_1NegotiateAuthenticatedChannelKeyExchange(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HAUTHCHANNEL hAuthChannel,
    UINT DataSize,
    VOID *pData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hAuthChannel, DataSize, pData);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1QueryAuthenticatedChannel(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HAUTHCHANNEL hAuthChannel,
    UINT InputDataSize,
    CONST VOID *pInputData,
    UINT OutputDataSize,
    VOID *pOutputData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hAuthChannel, InputDataSize, pInputData, OutputDataSize, pOutputData);
    return E_FAIL;
}

static HRESULT APIENTRY ddi11_1ConfigureAuthenticatedChannel(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HAUTHCHANNEL hAuthChannel,
    UINT InputDataSize,
    CONST VOID *pInputData,
    D3D11_1DDI_AUTHENTICATED_CONFIGURE_OUTPUT *pOutputData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hAuthChannel, InputDataSize, pInputData, pOutputData);
    return E_FAIL;
}

static HRESULT APIENTRY ddi11_1VideoDecoderGetHandle(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HDECODE hDecoder,
    HANDLE *pContentProtectionHandle)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hDecoder, pContentProtectionHandle);
    return S_OK;
}

static HRESULT APIENTRY ddi11_1CryptoSessionGetHandle(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_HCRYPTOSESSION hCryptoSession,
    HANDLE *pHandle)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, hCryptoSession, pHandle);
    return S_OK;
}

static VOID APIENTRY ddi11_1GetCaptureHandle(
    D3D10DDI_HDEVICE hDevice,
    D3D11_1DDI_GETCAPTUREHANDLEDATA *pHandleData)
{
    PVBOXDX_DEVICE pDevice = (PVBOXDX_DEVICE)hDevice.pDrvPrivate;
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice, pHandleData);
    return;
}

HRESULT ddi11_1RetrieveVideoFunctions(PVBOXDX_DEVICE pDevice, D3D11_1DDI_VIDEO_INPUT *pVideoInput)
{
    DEBUG_BREAKPOINT_TEST();
    RT_NOREF(pDevice);

    D3D11_1DDI_VIDEODEVICEFUNCS *p = pVideoInput->p11VideoDeviceFuncs;

    p->pfnGetVideoDecoderProfileCount                 = ddi11_1GetVideoDecoderProfileCount;
    p->pfnGetVideoDecoderProfile                      = ddi11_1GetVideoDecoderProfile;
    p->pfnCheckVideoDecoderFormat                     = ddi11_1CheckVideoDecoderFormat;
    p->pfnGetVideoDecoderConfigCount                  = ddi11_1GetVideoDecoderConfigCount;
    p->pfnGetVideoDecoderConfig                       = ddi11_1GetVideoDecoderConfig;
    p->pfnGetVideoDecoderBufferTypeCount              = ddi11_1GetVideoDecoderBufferTypeCount;
    p->pfnGetVideoDecoderBufferInfo                   = ddi11_1GetVideoDecoderBufferInfo;
    p->pfnCalcPrivateVideoDecoderSize                 = ddi11_1CalcPrivateVideoDecoderSize;
    p->pfnCreateVideoDecoder                          = ddi11_1CreateVideoDecoder;
    p->pfnDestroyVideoDecoder                         = ddi11_1DestroyVideoDecoder;
    p->pfnVideoDecoderExtension                       = ddi11_1VideoDecoderExtension;
    p->pfnVideoDecoderBeginFrame                      = ddi11_1VideoDecoderBeginFrame;
    p->pfnVideoDecoderEndFrame                        = ddi11_1VideoDecoderEndFrame;
    p->pfnVideoDecoderSubmitBuffers                   = ddi11_1VideoDecoderSubmitBuffers;
    p->pfnCalcPrivateVideoProcessorEnumSize           = ddi11_1CalcPrivateVideoProcessorEnumSize;
    p->pfnCreateVideoProcessorEnum                    = ddi11_1CreateVideoProcessorEnum;
    p->pfnDestroyVideoProcessorEnum                   = ddi11_1DestroyVideoProcessorEnum;
    p->pfnCheckVideoProcessorFormat                   = ddi11_1CheckVideoProcessorFormat;
    p->pfnGetVideoProcessorCaps                       = ddi11_1GetVideoProcessorCaps;
    p->pfnGetVideoProcessorRateConversionCaps         = ddi11_1GetVideoProcessorRateConversionCaps;
    p->pfnGetVideoProcessorCustomRate                 = ddi11_1GetVideoProcessorCustomRate;
    p->pfnGetVideoProcessorFilterRange                = ddi11_1GetVideoProcessorFilterRange;
    p->pfnCalcPrivateVideoProcessorSize               = ddi11_1CalcPrivateVideoProcessorSize;
    p->pfnCreateVideoProcessor                        = ddi11_1CreateVideoProcessor;
    p->pfnDestroyVideoProcessor                       = ddi11_1DestroyVideoProcessor;
    p->pfnVideoProcessorSetOutputTargetRect           = ddi11_1VideoProcessorSetOutputTargetRect;
    p->pfnVideoProcessorSetOutputBackgroundColor      = ddi11_1VideoProcessorSetOutputBackgroundColor;
    p->pfnVideoProcessorSetOutputColorSpace           = ddi11_1VideoProcessorSetOutputColorSpace;
    p->pfnVideoProcessorSetOutputAlphaFillMode        = ddi11_1VideoProcessorSetOutputAlphaFillMode;
    p->pfnVideoProcessorSetOutputConstriction         = ddi11_1VideoProcessorSetOutputConstriction;
    p->pfnVideoProcessorSetOutputStereoMode           = ddi11_1VideoProcessorSetOutputStereoMode;
    p->pfnVideoProcessorSetOutputExtension            = ddi11_1VideoProcessorSetOutputExtension;
    p->pfnVideoProcessorGetOutputExtension            = ddi11_1VideoProcessorGetOutputExtension;
    p->pfnVideoProcessorSetStreamFrameFormat          = ddi11_1VideoProcessorSetStreamFrameFormat;
    p->pfnVideoProcessorSetStreamColorSpace           = ddi11_1VideoProcessorSetStreamColorSpace;
    p->pfnVideoProcessorSetStreamOutputRate           = ddi11_1VideoProcessorSetStreamOutputRate;
    p->pfnVideoProcessorSetStreamSourceRect           = ddi11_1VideoProcessorSetStreamSourceRect;
    p->pfnVideoProcessorSetStreamDestRect             = ddi11_1VideoProcessorSetStreamDestRect;
    p->pfnVideoProcessorSetStreamAlpha                = ddi11_1VideoProcessorSetStreamAlpha;
    p->pfnVideoProcessorSetStreamPalette              = ddi11_1VideoProcessorSetStreamPalette;
    p->pfnVideoProcessorSetStreamPixelAspectRatio     = ddi11_1VideoProcessorSetStreamPixelAspectRatio;
    p->pfnVideoProcessorSetStreamLumaKey              = ddi11_1VideoProcessorSetStreamLumaKey;
    p->pfnVideoProcessorSetStreamStereoFormat         = ddi11_1VideoProcessorSetStreamStereoFormat;
    p->pfnVideoProcessorSetStreamAutoProcessingMode   = ddi11_1VideoProcessorSetStreamAutoProcessingMode;
    p->pfnVideoProcessorSetStreamFilter               = ddi11_1VideoProcessorSetStreamFilter;
    p->pfnVideoProcessorSetStreamExtension            = ddi11_1VideoProcessorSetStreamExtension;
    p->pfnVideoProcessorGetStreamExtension            = ddi11_1VideoProcessorGetStreamExtension;
    p->pfnVideoProcessorBlt                           = ddi11_1VideoProcessorBlt;
    p->pfnCalcPrivateVideoDecoderOutputViewSize       = ddi11_1CalcPrivateVideoDecoderOutputViewSize;
    p->pfnCreateVideoDecoderOutputView                = ddi11_1CreateVideoDecoderOutputView;
    p->pfnDestroyVideoDecoderOutputView               = ddi11_1DestroyVideoDecoderOutputView;
    p->pfnCalcPrivateVideoProcessorInputViewSize      = ddi11_1CalcPrivateVideoProcessorInputViewSize;
    p->pfnCreateVideoProcessorInputView               = ddi11_1CreateVideoProcessorInputView;
    p->pfnDestroyVideoProcessorInputView              = ddi11_1DestroyVideoProcessorInputView;
    p->pfnCalcPrivateVideoProcessorOutputViewSize     = ddi11_1CalcPrivateVideoProcessorOutputViewSize;
    p->pfnCreateVideoProcessorOutputView              = ddi11_1CreateVideoProcessorOutputView;
    p->pfnDestroyVideoProcessorOutputView             = ddi11_1DestroyVideoProcessorOutputView;
    p->pfnVideoProcessorInputViewReadAfterWriteHazard = ddi11_1VideoProcessorInputViewReadAfterWriteHazard;
    p->pfnGetContentProtectionCaps                    = ddi11_1GetContentProtectionCaps;
    p->pfnGetCryptoKeyExchangeType                    = ddi11_1GetCryptoKeyExchangeType;
    p->pfnCalcPrivateCryptoSessionSize                = ddi11_1CalcPrivateCryptoSessionSize;
    p->pfnCreateCryptoSession                         = ddi11_1CreateCryptoSession;
    p->pfnDestroyCryptoSession                        = ddi11_1DestroyCryptoSession;
    p->pfnGetCertificateSize                          = ddi11_1GetCertificateSize;
    p->pfnGetCertificate                              = ddi11_1GetCertificate;
    p->pfnNegotiateCryptoSessionKeyExchange           = ddi11_1NegotiateCryptoSessionKeyExchange;
    p->pfnEncryptionBlt                               = ddi11_1EncryptionBlt;
    p->pfnDecryptionBlt                               = ddi11_1DecryptionBlt;
    p->pfnStartSessionKeyRefresh                      = ddi11_1StartSessionKeyRefresh;
    p->pfnFinishSessionKeyRefresh                     = ddi11_1FinishSessionKeyRefresh;
    p->pfnGetEncryptionBltKey                         = ddi11_1GetEncryptionBltKey;
    p->pfnCalcPrivateAuthenticatedChannelSize         = ddi11_1CalcPrivateAuthenticatedChannelSize;
    p->pfnCreateAuthenticatedChannel                  = ddi11_1CreateAuthenticatedChannel;
    p->pfnDestroyAuthenticatedChannel                 = ddi11_1DestroyAuthenticatedChannel;
    p->pfnNegotiateAuthenticatedChannelKeyExchange    = ddi11_1NegotiateAuthenticatedChannelKeyExchange;
    p->pfnQueryAuthenticatedChannel                   = ddi11_1QueryAuthenticatedChannel;
    p->pfnConfigureAuthenticatedChannel               = ddi11_1ConfigureAuthenticatedChannel;
    p->pfnVideoDecoderGetHandle                       = ddi11_1VideoDecoderGetHandle;
    p->pfnCryptoSessionGetHandle                      = ddi11_1CryptoSessionGetHandle;
    p->pfnVideoProcessorSetStreamRotation             = ddi11_1VideoProcessorSetStreamRotation;
    p->pfnGetCaptureHandle                            = ddi11_1GetCaptureHandle;

    return S_OK;
}
