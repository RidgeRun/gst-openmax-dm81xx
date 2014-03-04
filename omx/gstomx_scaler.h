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

#ifndef GSTOMX_SCALER_H
#define GSTOMX_SCALER_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_SCALER(obj) (GstLegacyOmxScaler *) (obj)
#define GST_OMX_SCALER_TYPE (gst_omx_scaler_get_type ())

typedef struct GstLegacyOmxScaler GstLegacyOmxScaler;
typedef struct GstLegacyOmxScalerClass GstLegacyOmxScalerClass;

#include "gstomx_base_vfpc.h"

struct GstLegacyOmxScaler
{
    GstOmxBaseVfpc omx_base;
    gchar *crop_area;
    guint startX;
    guint startY;
    guint cropWidth;
    guint cropHeight;
};

struct GstLegacyOmxScalerClass
{
    GstOmxBaseVfpcClass parent_class;
};

GType gst_omx_scaler_get_type (void);

G_END_DECLS

#endif /* GSTOMX_SCALER_H */
