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

#include "gstomx_base_vfpc2.h"
#include "gstomx.h"
#include <gst/video/video.h>

#include <OMX_TI_Index.h>

#include <string.h> /* for memset */

GSTOMX_BOILERPLATE (GstOmxBaseVfpc2, gst_omx_base_vfpc2, GstOmxBaseFilter2, GST_OMX_BASE_FILTER2_TYPE);

static GstFlowReturn push_buffer (GstOmxBaseFilter2 *self, GstBuffer *buf);

static gboolean
pad_event (GstPad *pad, GstEvent *event)
{
    GstOmxBaseVfpc2 *self;
    GstOmxBaseFilter2 *omx_base;

    self = GST_OMX_BASE_VFPC2 (GST_OBJECT_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER2 (self);

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
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;
    GstOmxBaseFilter2Class *bfilter_class = GST_OMX_BASE_FILTER2_CLASS (g_class);

    element_class = GST_ELEMENT_CLASS (g_class);

    bfilter_class->pad_event = pad_event;
}

static GstFlowReturn
push_buffer (GstOmxBaseFilter2 *omx_base, GstBuffer *buf)
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
    GstOmxBaseVfpc2 *self;
    GstOmxBaseFilter2 *omx_base;
    GOmxCore *gomx;
    GstVideoFormat format;

    self = GST_OMX_BASE_VFPC2 (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER2 (self);

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

			if (self->framerate_num && self->framerate_denom) {
				omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,
						gst_value_get_fraction_denominator (framerate),
						gst_value_get_fraction_numerator (framerate));
			}
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
    GstOmxBaseVfpc2 *self;
    GstOmxBaseFilter2 *omx_base;
    GstVideoFormat format;
    GstStructure *structure;
	int i;

    self = GST_OMX_BASE_VFPC2 (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER2 (self);
    structure = gst_caps_get_structure (caps, 0);

    GST_INFO_OBJECT (omx_base, "setcaps (src): %" GST_PTR_FORMAT, caps);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

	for (i=0; i<NUM_OUTPUTS; i++) 
		if (pad == omx_base->srcpad[i]) break;
	if (!(i<NUM_OUTPUTS)) return FALSE;

    if (!gst_video_format_parse_caps_strided (caps,
            &format, &self->out_width[i], &self->out_height[i], &self->out_stride[i]))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!self->out_stride[i])
    {
        self->out_stride[i] = gstomx_calculate_stride (self->out_width[i], format);
    }

    /* save the src caps later needed by omx transport buffer */
    if (omx_base->out_port[i]->caps)
        gst_caps_unref (omx_base->out_port[i]->caps);

    omx_base->out_port[i]->caps = gst_caps_copy (caps);

    return TRUE;
}

static void
omx_setup (GstOmxBaseFilter2 *omx_base)
{
    GstOmxBaseVfpc2 *self;
    GOmxCore *gomx;
    GOmxPort *port;
	int i;

    self = GST_OMX_BASE_VFPC2 (omx_base);
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

	for (i=0; i<NUM_OUTPUTS; i++) {
		/* enable output port */
		port = omx_base->out_port[i];
		OMX_SendCommand (g_omx_core_get_handle (port->core),
				OMX_CommandPortEnable, port->port_index, NULL);
		g_sem_down (port->core->port_sem);
	}
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
    GST_OMX_BASE_FILTER2_CLASS (g_class)->push_buffer = push_buffer;
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter2 *omx_base;
    GstOmxBaseVfpc2 *self;
	char srcname[10];
	int i;

    omx_base = GST_OMX_BASE_FILTER2 (instance);
    self = GST_OMX_BASE_VFPC2 (instance);

    omx_base->omx_setup = omx_setup;
    self->g_class = g_class;

    gst_pad_set_setcaps_function (omx_base->sinkpad,
            GST_DEBUG_FUNCPTR (sink_setcaps));
	for (i=0; i<NUM_OUTPUTS; i++) 
    	gst_pad_set_setcaps_function (omx_base->srcpad[i],
            GST_DEBUG_FUNCPTR (src_setcaps));
	
    /* free the existing core and ports */
    g_omx_core_free (omx_base->gomx);
    g_omx_port_free (omx_base->in_port);

    /* create new core and ports */
    omx_base->gomx = g_omx_core_new (omx_base, self->g_class);
    omx_base->in_port = g_omx_core_get_port (omx_base->gomx, "in", OMX_VFPC_INPUT_PORT_START_INDEX);

    omx_base->in_port->omx_allocate = TRUE;
    omx_base->in_port->share_buffer = FALSE;
    omx_base->in_port->always_copy  = FALSE;
	omx_base->in_port->port_index = OMX_VFPC_INPUT_PORT_START_INDEX;

	for (i=0; i<NUM_OUTPUTS; i++) {
    	g_omx_port_free (omx_base->out_port[i]);
		sprintf(srcname, "out_%02x", i);
    	omx_base->out_port[i] = g_omx_core_get_port (omx_base->gomx, srcname, 
		   OMX_VFPC_OUTPUT_PORT_START_INDEX + i);
		omx_base->out_port[i]->port_index = OMX_VFPC_OUTPUT_PORT_START_INDEX + i;
		omx_base->out_port[i]->omx_allocate = TRUE;
		omx_base->out_port[i]->share_buffer = FALSE;
		omx_base->out_port[i]->always_copy = FALSE;
	}
}

