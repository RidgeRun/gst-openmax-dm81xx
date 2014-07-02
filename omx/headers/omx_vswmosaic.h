/*
 *  Copyright (c) 2010-2011, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact information for paper mail:
 *  Texas Instruments
 *  Post Office Box 655303
 *  Dallas, Texas 75265
 *  Contact information:
 *  http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
 *  DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
 *  ============================================================================
 *
 */

/**
*   @file  omx_vswmosaic.h
 *   This file contains API/Defines that provides the functionality for the
 *   {V}ideo {L}oo{P}{B}ack component.
 *   - A user would typically use functions and data structures defined in this
 *   module to access different VSWMOSAIC functionalites.
 *
 *   @path \ti\omx\comp\vswmosaic\
*
*   @rev  1.0
 */

/** ===========================================================================
 *! Revision History
 *! ===========================================================================
 *!17-October-2009 : Initial Version
 * ============================================================================
 */

#ifndef _OMX_VSWMOSAIC_H
#define _OMX_VSWMOSAIC_H

#ifdef _cplusplus
extern "C"
{
#endif                          /* __cplusplus */

  /* -------------------compilation control switches ----------------------- */
/* none */
  /* ----------------------------------------------------------------------- */

/*****************************************************************************
 * -------------------INCLUDE FILES-------------------------------------------
 *****************************************************************************/
  /* ------------------ system and platform files -------------------------- */
/* none */
/*--------------------program files -----------------------------------------*/
#include "OMX_Types.h"

/*****************************************************************************
 * PUBLIC DECLARATIONS Defined here, used elsewhere
 *****************************************************************************/

/* Component name for VSWMOSAIC. This component can be compiled for any core - A8
 * or DSP. Therefore, it has different names to uniquely identify the
 * dsp VSWMOSAIC component and A8 VSWMOSAIC component
 */

/* Component name on VPSS-M3  */
#define OMX_VSWMOSAIC_COMP_NAME ((OMX_STRING) "OMX.TI.VPSSM3.VSWMOSAIC")
/*--------------------------Data declarations --------------------------------*/

/* ========================================================================== */
/* Macros & Typedefs                                                          */
/* ========================================================================== */
  
 /** Default port start number of VSWMOSAIC comp */
#define OMX_VSWMOSAIC_DEFAULT_START_PORT_NUM (0)

 /** Input port Index for the VSWMOSAIC OMX Comp */
#define OMX_VSWMOSAIC_INPUT_PORT_START_INDEX (OMX_VSWMOSAIC_DEFAULT_START_PORT_NUM)

 /** Maximum Number of input ports for the VSWMOSAIC Comp */
#define OMX_VSWMOSAIC_NUM_INPUT_PORTS        (16)

 /** Output port Index for the VSWMOSAIC OMX Comp */
#define OMX_VSWMOSAIC_OUTPUT_PORT_START_INDEX  (OMX_VSWMOSAIC_DEFAULT_START_PORT_NUM +   \
                                           OMX_VSWMOSAIC_NUM_INPUT_PORTS)

 /** Maximum Number of output ports for the VSWMOSAIC Component */
#define OMX_VSWMOSAIC_NUM_OUTPUT_PORTS       (1)

/** \brief Maximum number of mosaic windows supported in a single VSWMOSAIC instance */
#define OMX_VSWMOSAIC_MAX_NUM_DISPLAY_WINDOWS  (16u)

/**
 *  \brief This macro determines the maximum number of planes/address used to
 *  represent a video buffer per field.
 *
 *  Currently this is set to 3 to support the maximum pointers required for
 *  YUV planar format - Y, Cb and Cr.
 */
#define OMX_VSWMOSAIC_MAX_PLANES                 (3u)

/** \brief Index for YUV444/YUV422 interleaved formats. */
#define OMX_VSWMOSAIC_YUV_INT_ADDR_IDX           (0u)

/** \brief Y Index for YUV semi planar formats. */
#define OMX_VSWMOSAIC_YUV_SP_Y_ADDR_IDX          (0u)

/** \brief CB Index for semi planar formats. */
#define OMX_VSWMOSAIC_YUV_SP_CBCR_ADDR_IDX       (1u)


/*******************************************************************************
* Enumerated Types
*******************************************************************************/

/*******************************************************************************
* Structures
*******************************************************************************/

 /**
 *  struct OMX_VSWMOSAIC_MultipleWindowFmt
 *  \brief Structure for setting the mosaic or region based graphic
 *  window for each of the window.
 *
 *  @ param winStartX : Horizontal start
 *  @ param winStartY : Vertical start
 *  @ param winWidth  : Width in pixels
 *  @ param winHeight : Number of lines in a window. For interlaced mode, this
 *                      should be set to the frame size and not the field size
 *  @ param pitch     : Pitch in bytes for each of the sub-window buffers. This
 *                      represents the difference between two consecutive line
 *                      address. This is irrespective of whether the video is
 *                      interlaced or progressive and whether the fields are
 *                      merged or separated for interlaced video
 *  @ param dataFormat: Data format for each window
 *  @ param priority  : In case of overlapping windows (as in PIP), priority
 *                      could be used to choose the window to be displayed in
 *                      the overlapped region. 0 is lowest layer, 1 is next
 *                      and so on...Note that keeping same priority for all
 *                      windows specifies that there are no overlapping windows
 */
typedef struct OMX_VSWMOSAIC_MultipleWindowFmt
{
    OMX_U32                    winStartX;
    OMX_U32                    winStartY;
    OMX_U32                    winWidth;
    OMX_U32                    winHeight;
    OMX_U32                    pitch [OMX_VSWMOSAIC_MAX_PLANES];
    OMX_COLOR_FORMATTYPE       dataFormat;
    OMX_U32                    nPortIndex;
} OMX_VSWMOSAIC_MultipleWindowFmt;

//TODO: remove this.
/** OMX_CONFIG_VSWMOSAIC_MOSAICLAYOUT_PORT2WINMAP : Mapping OMX port to window
 *
 *  @ param nSize      : Size of the structure in bytes
 *  @ param nVersion   : OMX specification version information 
 *  @ param nPortIndex : Index of the port
 *  @ param nlayoutId  : ID of the mosaic layout
 *  @ param numWindows : Number of display windows in the mosaic layout
 *  @ param omxPortList: OMX port list in which the OMX ports are listed 
 *                       in the same order as the dipslay windows order
 */
typedef struct OMX_CONFIG_VSWMOSAIC_MOSAICLAYOUT_PORT2WINMAP
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nLayoutId;
    OMX_U32 numWindows;
    OMX_U32 omxPortList[OMX_VSWMOSAIC_MAX_NUM_DISPLAY_WINDOWS];
} OMX_CONFIG_VSWMOSAIC_MOSAICLAYOUT_PORT2WINMAP;


/** OMX_PARAM_VSWMOSAIC_CREATEMOSAICLAYOUT: Structure containing a specific  
 *                                      Mosaic layout info
 * 
 *  @ param nSize         : Size of the structure in bytes
 *  @ param nVersion      : OMX specification version information 
 *  @ param nPortIndex    : Index of the port
 *  @ param nlayoutId     : ID of the mosaic layout
 *  @ param  nNumWindows   : Specifies the number windows that would require to
 *                          be displayed,MosaicWinFmt should point to a array
 *                          that has atleast numWindows of entries
 *  @ param  sMosaicWinFmt : FVID2 display driver Mosaic window format
 */
typedef struct OMX_CONFIG_VSWMOSAIC_CREATEMOSAICLAYOUT
{
    OMX_U32               nSize;
    OMX_VERSIONTYPE       nVersion;
    OMX_U32               nPortIndex;
    OMX_U32               nOpWidth;
    OMX_U32               nOpHeight;
    OMX_U32               nOpPitch;
    OMX_U32               nNumWindows;
    OMX_VSWMOSAIC_MultipleWindowFmt 
                          sMosaicWinFmt[OMX_VSWMOSAIC_MAX_NUM_DISPLAY_WINDOWS];
}  OMX_CONFIG_VSWMOSAIC_CREATEMOSAICLAYOUT;

/** OMX_PARAM_VSWMOSAIC_SETBACKGROUNDCOLOR: Structure containing a specific  
 *                                      Background color
 * 
 *  @ param nColorId         : Background color Id
 */
typedef struct OMX_CONFIG_VSWMOSAIC_SETBACKGROUNDCOLOR
{
    OMX_U32               nSize;
    OMX_VERSIONTYPE       nVersion;
    OMX_U32               nPortIndex;
    OMX_U32               uColor; /*In YCbYCr format*/
} OMX_CONFIG_VSWMOSAIC_SETBACKGROUNDCOLOR;

/** OMX_CONFIG_VSWMOSAIC_PORT_PROPERTY: Structure containing specific  
 *                                      input/output ports properties
 *
 *  @ param  nPortIndex  : port index
 *  @ param  nDataFormat : YUV422I or YUV420SP or YUV422SP
 *  @ param  nScanFormat : Always progressive for EDMA based
 *  @ param  nWidth      : Width
 *  @ param  nHeight     : Height
 *  @ param  nStartX     : Valid start coordinition x (No pixel alignment restrictions)
 *  @ param  nStartY     : Valid start coordinition y
 *  @ param  nPitchY     : Y stride 
 *  @ param  nPitchC     : C stride 
 *  @ param  nMemoryType : Always non tiled for EDMA based
 */
typedef struct OMX_CONFIG_VSWMOSAIC_PORT_PROPERTY
{
  OMX_U32 nPortIndex;
  OMX_U32 nDataFormat;
  OMX_U32 nScanFormat;
  OMX_U32 nWidth;
  OMX_U32 nHeight;
  OMX_U32 nStartX;
  OMX_U32 nStartY;
  OMX_U32 nPitchY;
  OMX_U32 nPitchC;
  OMX_U32 nMemoryType;
}  OMX_CONFIG_VSWMOSAIC_PORT_PROPERTY;

/** OMX_CONFIG_VSWMOSAIC_OUTPUT_RESOLUTION: Structure containing specific  
 *                                      output Resolution
 * 
 *  @ param  nPortIndex  : output port index 
 *  @ param  nWidth      : resolution width 
 *  @ param  nHeight     : resolution height
 */
typedef struct OMX_CONFIG_VSWMOSAIC_OUTPUT_RESOLUTION
{
  OMX_U32 nPortIndex;
  OMX_U32 nWidth;
  OMX_U32 nHeight;
}  OMX_CONFIG_VSWMOSAIC_OUTPUT_RESOLUTION;

/** OMX_PARAM_VSWMOSAIC_MOSAIC_PERIODICITY: Structure containing specific  
 *                                      mosaic periodicity
 * 
 *  @ param nPeriodicity :  mosaic fps
 */
typedef struct OMX_PARAM_VSWMOSAIC_MOSAIC_PERIODICITY
{
    OMX_U32               nSize;
    OMX_VERSIONTYPE       nVersion;
    OMX_U32               nPortIndex;
    OMX_U32               nFps;
}  OMX_PARAM_VSWMOSAIC_MOSAIC_PERIODICITY;

/*----------------------function prototypes ---------------------------------*/

/** OMX VSWMOSAIC Component Init */
OMX_ERRORTYPE OMX_TI_VSWMOSAIC_ComponentInit (OMX_HANDLETYPE hComponent);

#ifdef _cplusplus
}
#endif /* __cplusplus */

#endif /* _OMX_VSWMOSAIC_H */

/* omx_vswmosaic.h - EOF */
