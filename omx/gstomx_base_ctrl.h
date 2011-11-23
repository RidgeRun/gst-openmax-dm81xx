/*
 * Copyright (C) 2010-2011 Texas Instrument Inc.
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

#ifndef GSTOMX_BASE_CTRL_H
#define GSTOMX_BASE_CTRL_H

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfdc.h>
#include <omx_ctrl.h>


G_BEGIN_DECLS

#define GST_OMX_BASE_CTRL(obj) (GstOmxBaseCtrl *) (obj)
#define GST_OMX_BASE_CTRL_TYPE (gst_omx_base_ctrl_get_type ())
#define GST_OMX_BASE_CTRL_CLASS(obj) (GstOmxBaseCtrlClass *) (obj)

typedef struct GstOmxBaseCtrl GstOmxBaseCtrl;
typedef struct GstOmxBaseCtrlClass GstOmxBaseCtrlClass;

#include <gstomx_util.h>

struct GstOmxBaseCtrl
{
    GstBaseTransform  element;
    GstPad  *sinkpad;
    GstPad  *srcpad;

    GOmxCore *gomx;
    GOmxPort *in_port;

    char *omx_role;
    char *omx_component;
    char *omx_library;
    char *display_mode;

    gboolean mode_configured;
	char *display_device;
};

struct GstOmxBaseCtrlClass
{
    GstBaseTransformClass parent_class;
};

GType gst_omx_base_ctrl_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_CTRL_H */

