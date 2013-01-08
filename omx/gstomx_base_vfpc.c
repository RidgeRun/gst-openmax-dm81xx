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

#include "gstomx_base_vfpc.h"
#include "gstomx.h"
#include <gst/video/video.h>

#include <OMX_TI_Index.h>

#include <string.h> /* for memset */

enum
{
    ARG_0,
    ARG_PORT_INDEX,
};

GSTOMX_BOILERPLATE (GstOmxBaseVfpc, gst_omx_base_vfpc, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);

static GstFlowReturn push_buffer (GstOmxBaseFilter *self, GstBuffer *buf);

static gboolean
pad_event (GstPad *pad, GstEvent *event)
{
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;

    self = GST_OMX_BASE_VFPC (GST_OBJECT_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    GST_INFO_OBJECT (self, "begin: event=%s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_CROP:
        {
            gst_event_parse_crop (event, &self->top, &self->left, NULL, NULL);
            return TRUE;
        }
        default:
        {
            return parent_class->pad_event (pad, event);
        }
    }
}

static void
gstomx_vfpc_set_port_index (GObject *obj, int index)
{
    GstOmxBaseFilter *omx_base;
    GstOmxBaseVfpc *self;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_BASE_VFPC (obj);

    self->input_port_index = OMX_VFPC_INPUT_PORT_START_INDEX + index;
    self->output_port_index = OMX_VFPC_OUTPUT_PORT_START_INDEX + index;
 
    /* free the existing core and ports */
    g_omx_core_free (omx_base->gomx);
    g_omx_port_free (omx_base->in_port);
    g_omx_port_free (omx_base->out_port);

    /* create new core and ports */
    omx_base->gomx = g_omx_core_new (omx_base, self->g_class);
    omx_base->in_port = g_omx_core_get_port (omx_base->gomx, "in", self->input_port_index);
    omx_base->out_port = g_omx_core_get_port (omx_base->gomx, "out", self->output_port_index);

    omx_base->in_port->omx_allocate = TRUE;
    omx_base->in_port->share_buffer = FALSE;
    omx_base->in_port->always_copy  = FALSE;

    omx_base->out_port->omx_allocate = TRUE;
    omx_base->out_port->share_buffer = FALSE;
    omx_base->out_port->always_copy = FALSE;

    omx_base->in_port->port_index = self->input_port_index;
    omx_base->out_port->port_index = self->output_port_index;
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;
    GstOmxBaseFilterClass *bfilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);

    element_class = GST_ELEMENT_CLASS (g_class);

    bfilter_class->pad_event = pad_event;
}

static GstFlowReturn
push_buffer (GstOmxBaseFilter *omx_base, GstBuffer *buf)
{
    return parent_class->push_buffer (omx_base, buf);
}

static gint
gstomx_calculate_stride (int width, GstVideoFormat format)
{
    switch (format)
    {
        case GST_VIDEO_FORMAT_NV12:
            return width;
        case GST_VIDEO_FORMAT_YUY2:
            return width * 2;
        default:
            GST_ERROR ("unsupported color format");
    }
    return -1;
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;
    GOmxCore *gomx;
    GstVideoFormat format;

    self = GST_OMX_BASE_VFPC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    structure = gst_caps_get_structure (caps, 0);

    g_return_val_if_fail (structure, FALSE);

    if (!gst_video_format_parse_caps_strided (caps,
            &format, &self->in_width, &self->in_height, &self->in_stride))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!self->in_stride) 
    {
        self->in_stride = gstomx_calculate_stride (self->in_width, format);
    }

    {
        const GValue *framerate = NULL;
        framerate = gst_structure_get_value (structure, "framerate");
        if (framerate)
        {
            self->framerate_num = gst_value_get_fraction_numerator (framerate);
            self->framerate_denom = gst_value_get_fraction_denominator (framerate);

            omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,
                    gst_value_get_fraction_denominator (framerate),
                    gst_value_get_fraction_numerator (framerate));
            GST_DEBUG_OBJECT (self, "Nominal frame duration =%"GST_TIME_FORMAT,
                                GST_TIME_ARGS (omx_base->duration));
        }
    }
	/* check for pixel-aspect-ratio, to set to src caps */
    {
        const GValue *v = NULL;
        v = gst_structure_get_value (structure, "pixel-aspect-ratio");
        if (v) {
            self->pixel_aspect_ratio_num = gst_value_get_fraction_numerator (v);
            self->pixel_aspect_ratio_denom = gst_value_get_fraction_denominator (v);
		} else self->pixel_aspect_ratio_denom = 0;
    }

	if (!gst_structure_get_boolean (structure, "interlaced", &self->interlaced))
		self->interlaced = FALSE;

    if (self->sink_setcaps)
        self->sink_setcaps (pad, caps);

    return gst_pad_set_caps (pad, caps);
}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;
    GstVideoFormat format;
    GstStructure *structure;

    self = GST_OMX_BASE_VFPC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);
    structure = gst_caps_get_structure (caps, 0);

    GST_INFO_OBJECT (omx_base, "setcaps (src): %" GST_PTR_FORMAT, caps);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    if (!gst_video_format_parse_caps_strided (caps,
            &format, &self->out_width, &self->out_height, &self->out_stride))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!self->out_stride)
    {
        self->out_stride = gstomx_calculate_stride (self->out_width, format);
    }

    /* save the src caps later needed by omx transport buffer */
    if (omx_base->out_port->caps)
        gst_caps_unref (omx_base->out_port->caps);

    omx_base->out_port->caps = gst_caps_copy (caps);

    return TRUE;
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_BASE_VFPC (obj);

    switch (prop_id)
    {
        case ARG_PORT_INDEX:
            self->port_index = g_value_get_uint (value);
            if (!self->port_configured) 
                gstomx_vfpc_set_port_index (obj, self->port_index);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
get_property (GObject *obj,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseVfpc *self;

    self = GST_OMX_BASE_VFPC (obj);

    switch (prop_id)
    {
        case ARG_PORT_INDEX:
            g_value_set_uint (value, self->port_index);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVfpc *self;
    GOmxCore *gomx;
    GOmxPort *port;

    self = GST_OMX_BASE_VFPC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    if (self->omx_setup)
    {
        self->omx_setup (omx_base);
    }
    
    /* enable input port */
    port = omx_base->in_port;
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    /* enable output port */
    port = omx_base->out_port;
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    /* indicate that port is now configured */
    self->port_configured = TRUE;

    GST_INFO_OBJECT (omx_base, "end");
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (g_class);
    GST_OMX_BASE_FILTER_CLASS (g_class)->push_buffer = push_buffer;

    /* Properties stuff */
    {
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;

        g_object_class_install_property (gobject_class, ARG_PORT_INDEX,
                                         g_param_spec_uint ("port-index", "port index",
                                                            "input/output start port index",
                                                            0, 8, 0, G_PARAM_READWRITE));
    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;
    GstOmxBaseVfpc *self;

    omx_base = GST_OMX_BASE_FILTER (instance);
    self = GST_OMX_BASE_VFPC (instance);

    omx_base->omx_setup = omx_setup;
    self->g_class = g_class;

    gst_pad_set_setcaps_function (omx_base->sinkpad,
            GST_DEBUG_FUNCPTR (sink_setcaps));
    gst_pad_set_setcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_setcaps));
}

