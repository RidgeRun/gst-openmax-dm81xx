/*
 * Copyright (C) 2011-2012 Texas Instruments Inc.
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

#ifndef GSTOMX_DEISCALER_H
#define GSTOMX_DEISCALER_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_DEISCALER(obj) (struct GstOmxDeiScaler *) (obj)
// #define GST_OMX_DEISCALER_TYPE (gst_omx_deiscaler_get_type ())

typedef struct GstOmxDeiScaler GstOmxMDeiScaler;
typedef struct GstOmxDeiScalerClass GstOmxMDeiScalerClass;
typedef struct GstOmxDeiScaler GstOmxHDeiScaler;
typedef struct GstOmxDeiScalerClass GstOmxHDeiScalerClass;

#include "gstomx_base_vfpc2.h"

struct GstOmxDeiScaler
{
    GstOmxBaseVfpc2 omx_base;
	gint framerate_divisor;
	guint framecnt[NUM_OUTPUTS];
};

struct GstOmxDeiScalerClass
{
    GstOmxBaseVfpc2Class parent_class;
};

GType gst_omx_hdeiscaler_get_type (void);
GType gst_omx_mdeiscaler_get_type (void);

G_END_DECLS

#endif /* GSTOMX_DEISCALER_H */
