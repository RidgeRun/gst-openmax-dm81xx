/*
 * Copyright (C) 2010-2011 Texas Instruments Inc.
 *
 * Author: Brijesh Singh <bksingh@ti.com>
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

#ifndef GSTOMX_BASE_VFPC2_H
#define GSTOMX_BASE_VFPC2_H

#include <gst/gst.h>

#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfpc.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_VFPC2(obj) (GstOmxBaseVfpc2 *) (obj)
#define GST_OMX_BASE_VFPC2_TYPE (gst_omx_base_vfpc2_get_type ())
#define GST_OMX_BASE_VFPC2_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), GST_OMX_BASE_VFPC2_TYPE, GstOmxBaseVfpc2Class))

typedef struct GstOmxBaseVfpc2 GstOmxBaseVfpc2;
typedef struct GstOmxBaseVfpc2Class GstOmxBaseVfpc2Class;

#include "gstomx_base_filter2.h"

struct GstOmxBaseVfpc2
{
    GstOmxBaseFilter2 omx_base;

    gint framerate_num;
    gint framerate_denom;
    gboolean port_configured;
    GstPadSetCapsFunction sink_setcaps;
    gint in_width, in_height, in_stride;
    gint out_width[NUM_OUTPUTS], out_height[NUM_OUTPUTS], out_stride[NUM_OUTPUTS];
    gint left, top;
    GstOmxBaseFilter2Cb omx_setup;
    gpointer g_class;
	gint pixel_aspect_ratio_num;
	gint pixel_aspect_ratio_denom;
	gboolean interlaced;
};

struct GstOmxBaseVfpc2Class
{
    GstOmxBaseFilter2Class parent_class;
};

GType gst_omx_base_vfpc2_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_VFPC2_H */

