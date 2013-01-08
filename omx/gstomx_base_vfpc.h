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

#ifndef GSTOMX_BASE_VFPC_H
#define GSTOMX_BASE_VFPC_H

#include <gst/gst.h>

#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfpc.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_VFPC(obj) (GstOmxBaseVfpc *) (obj)
#define GST_OMX_BASE_VFPC_TYPE (gst_omx_base_vfpc_get_type ())
#define GST_OMX_BASE_VFPC_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), GST_OMX_BASE_VFPC_TYPE, GstOmxBaseVfpcClass))

typedef struct GstOmxBaseVfpc GstOmxBaseVfpc;
typedef struct GstOmxBaseVfpcClass GstOmxBaseVfpcClass;

#include "gstomx_base_filter.h"

struct GstOmxBaseVfpc
{
    GstOmxBaseFilter omx_base;

    gint framerate_num;
    gint framerate_denom;
    gboolean port_configured;
    GstPadSetCapsFunction sink_setcaps;
    gint in_width, in_height, in_stride;
    gint out_width, out_height, out_stride;
    gint left, top;
    gint port_index, input_port_index, output_port_index;
    GstOmxBaseFilterCb omx_setup;
    gpointer g_class;
	gint pixel_aspect_ratio_num;
	gint pixel_aspect_ratio_denom;
	gboolean interlaced;
};

struct GstOmxBaseVfpcClass
{
    GstOmxBaseFilterClass parent_class;
};

GType gst_omx_base_vfpc_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_VFPC_H */

