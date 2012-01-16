/*
 * Copyright (C) 2011 RidgeRun.
 *
 * Author: David Soto <david.soto@ridgerun.com>
 *
 * Based on gstomx_base_ctrl.h made by:
 *       Brijesh Singh <bksingh@ti.com>
 *
 * Copyright (C) 2010-2011 Texas Instrument Inc.
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

#ifndef GSTOMX_TVP_H
#define GSTOMX_TVP_H

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <xdc/std.h>
#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfdc.h>
#include <omx_ctrl.h>


G_BEGIN_DECLS
#define GST_OMX_TVP(obj) (GstOmxBaseTvp *) (obj)
#define GST_OMX_TVP_TYPE (gst_omx_tvp_get_type ())
#define GST_OMX_TVP_CLASS(obj) (GstOmxBaseTvpClass *) (obj)
typedef struct GstOmxBaseTvp GstOmxBaseTvp;
typedef struct GstOmxBaseTvpClass GstOmxBaseTvpClass;

#include <gstomx_util.h>

struct GstOmxBaseTvp
{
  GstBaseTransform element;
  GstPad *sinkpad;
  GstPad *srcpad;

  GOmxCore *gomx;
  GOmxPort *in_port;

  gint width, height, rowstride;
  GstVideoFormat format;

  char *omx_role;
  char *omx_component;
  char *omx_library;
  gint input_interface;
  gint std;
  gint cap_mode;
  gint input_format;
  gint scan_type;

  gboolean mode_configured;
};

struct GstOmxBaseTvpClass
{
  GstBaseTransformClass parent_class;
};

GType gst_omx_tvp_get_type (void);

G_END_DECLS
#endif /* GSTOMX_TVP_H */
