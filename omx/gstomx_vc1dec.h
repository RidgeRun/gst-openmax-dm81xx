/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef GSTOMX_VC1DEC_H
#define GSTOMX_VC1DEC_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_VC1DEC(obj) (GstOmxVC1Dec *) (obj)
#define GST_OMX_VC1DEC_TYPE (gst_omx_vc1dec_get_type ())

typedef struct GstOmxVC1Dec GstOmxVC1Dec;
typedef struct GstOmxVC1DecClass GstOmxVC1DecClass;

#include "gstomx_base_videodec.h"

typedef struct VC1StructA
{
   OMX_U32 width;
   OMX_U32 height;  
}VC1StructA;

typedef struct VC1SequenceHdr
{
   OMX_U32 nFrames;
   OMX_U32 resv1;
   OMX_U32 StructC;
   VC1StructA StructA;
   OMX_U32 resv2;
   OMX_U32 StructB[3];
}VC1SequenceHdr;

struct GstOmxVC1Dec
{
    GstOmxBaseVideoDec omx_base;
};

struct GstOmxVC1DecClass
{
    GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_vc1dec_get_type (void);

G_END_DECLS

#endif /* GSTOMX_VC1DEC_H */
