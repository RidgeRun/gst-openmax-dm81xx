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

#ifndef GSTOMX_BASE_FILTER21_H
#define GSTOMX_BASE_FILTER21_H

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_FILTER21(obj) ((GstOmxBaseFilter21 *) (obj))
#define GST_OMX_BASE_FILTER21_TYPE (gst_omx_base_filter21_get_type ())
#define GST_OMX_BASE_FILTER21_CLASS(obj) ((GstOmxBaseFilter21Class *) (obj))
#define GST_OMX_BASE_FILTER21_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_OMX_BASE_FILTER21_TYPE, GstOmxBaseFilter21Class))

typedef struct GstOmxBaseFilter21 GstOmxBaseFilter21;
typedef struct GstOmxBaseFilter21Class GstOmxBaseFilter21Class;
typedef void (*GstOmxBaseFilter21Cb) (GstOmxBaseFilter21 *self);
typedef gboolean (*GstOmxBaseFilter21PushCb) (GstOmxBaseFilter21 *self, GstBuffer *buf);

#include "gstomx_util.h"
#include <async_queue.h>
#include <omx_vswmosaic.h>


struct GstOmxBaseFilter21
{
    GstElement element;
    
#define NUM_INPUTS 2
    GstPad *sinkpad[NUM_INPUTS];
    GstPad *srcpad;

    GstCollectPads *collectpads;

    GOmxCore *gomx;
    GOmxPort *in_port[NUM_INPUTS];
    GOmxPort *out_port;

    char *omx_role;
    char *omx_component;
    char *omx_library;
    gboolean ready;
    GMutex *ready_lock;
	gint in_width[NUM_INPUTS];
	gint in_height[NUM_INPUTS];
	gint in_stride[NUM_INPUTS];
    gint out_width, out_height, out_stride;
    GValue *out_framerate;
    gint x[NUM_INPUTS];
    gint y[NUM_INPUTS];
    GstOmxBaseFilter21Cb omx_setup;
    GstOmxBaseFilter21PushCb push_cb;
    GstFlowReturn last_pad_push_return;
    GstBuffer *codec_data;
    GstClockTime duration;
    GstClockTime last_buf_timestamp;
    GstClockTime sink_camera_timestamp;
	gint port_index, input_port_index[NUM_INPUTS], output_port_index;
	int number_eos;

};

struct GstOmxBaseFilter21Class
{
    GstElementClass parent_class;

    GstFlowReturn (*push_buffer) (GstOmxBaseFilter21 *self, GstBuffer *buf);
    GstFlowReturn (*pad_chain) (GstPad *pad, GstBuffer *buf);
    gboolean (*pad_event) (GstPad *pad, GstEvent *event);
};

GType gst_omx_base_filter21_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_FILTER2_H */
