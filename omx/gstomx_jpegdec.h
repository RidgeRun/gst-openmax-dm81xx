/*
 * Copyright (C) 2009 Texas Instruments - http://www.ti.com/
 *
 * Author:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef GSTOMX_JPEGDEC_H
#define GSTOMX_JPEGDEC_H

#include <gst/gst.h>
#include <gst/video/video.h>

#include <config.h>

G_BEGIN_DECLS

#define GST_OMX_JPEGDEC(obj) (GstOmxJpegDec *) (obj)
#define GST_OMX_JPEGDEC_TYPE (gst_omx_jpegdec_get_type ())

typedef struct GstOmxJpegDec GstOmxJpegDec;
typedef struct GstOmxJpegDecClass GstOmxJpegDecClass;

#include "gstomx_base_filter.h"

typedef struct OMX_CUSTOM_IMAGE_DECODE_SECTION
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nMCURow;
    OMX_U32 nAU;
    OMX_BOOL bSectionsInput;
    OMX_BOOL bSectionsOutput;
}OMX_CUSTOM_IMAGE_DECODE_SECTION;

typedef struct OMX_CUSTOM_IMAGE_DECODE_SUBREGION
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nXOrg;         /*Sectional decoding: X origin*/
    OMX_U32 nYOrg;         /*Sectional decoding: Y origin*/
    OMX_U32 nXLength;      /*Sectional decoding: X lenght*/
    OMX_U32 nYLength;      /*Sectional decoding: Y lenght*/
}OMX_CUSTOM_IMAGE_DECODE_SUBREGION;


typedef struct OMX_CUSTOM_RESOLUTION
{
    OMX_U32 nWidth;
    OMX_U32 nHeight;
}OMX_CUSTOM_RESOLUTION;

struct GstOmxJpegDec
{
    GstOmxBaseFilter omx_base;
    gint framerate_num;
    gint framerate_denom;
    gboolean progressive;
    gboolean outport_configured;

};

struct GstOmxJpegDecClass
{
    GstOmxBaseFilterClass parent_class;
};

GType gst_omx_jpegdec_get_type (void);

G_END_DECLS

#endif /* GSTOMX_JPEGEDEC_H */
