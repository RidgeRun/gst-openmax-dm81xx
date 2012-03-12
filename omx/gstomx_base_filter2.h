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

#ifndef GSTOMX_BASE_FILTER2_H
#define GSTOMX_BASE_FILTER2_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_FILTER2(obj) ((GstOmxBaseFilter2 *) (obj))
#define GST_OMX_BASE_FILTER2_TYPE (gst_omx_base_filter2_get_type ())
#define GST_OMX_BASE_FILTER2_CLASS(obj) ((GstOmxBaseFilter2Class *) (obj))
#define GST_OMX_BASE_FILTER2_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_OMX_BASE_FILTER2_TYPE, GstOmxBaseFilter2Class))

typedef struct GstOmxBaseFilter2 GstOmxBaseFilter2;
typedef struct GstOmxBaseFilter2Class GstOmxBaseFilter2Class;
typedef void (*GstOmxBaseFilter2Cb) (GstOmxBaseFilter2 *self);
typedef gboolean (*GstOmxBaseFilter2PushCb) (GstOmxBaseFilter2 *self, GstBuffer *buf);

#include "gstomx_util.h"
#include <async_queue.h>


struct GstOmxBaseFilter2
{
    GstElement element;

    GstPad *sinkpad;
#define NUM_OUTPUTS 2
    GstPad *srcpad[NUM_OUTPUTS];

    GOmxCore *gomx;
    GOmxPort *in_port;
    GOmxPort *out_port[NUM_OUTPUTS];

    char *omx_role;
    char *omx_component;
    char *omx_library;
    gboolean ready;
    GMutex *ready_lock;

    GstOmxBaseFilter2Cb omx_setup;
    GstOmxBaseFilter2PushCb push_cb;
    GstFlowReturn last_pad_push_return;
    GstBuffer *codec_data;
    GstClockTime duration;
    GstClockTime last_buf_timestamp[NUM_OUTPUTS];

   /* Used in deinterlacer kind of components where 
   	  one input interlaced input buffer in the input 
	  translates to 2 inputs to omx dei component 
	*/
    gboolean input_fields_separately;
	gint second_field_offset;
};

struct GstOmxBaseFilter2Class
{
    GstElementClass parent_class;

    GstFlowReturn (*push_buffer) (GstOmxBaseFilter2 *self, GstBuffer *buf);
    GstFlowReturn (*pad_chain) (GstPad *pad, GstBuffer *buf);
    gboolean (*pad_event) (GstPad *pad, GstEvent *event);
};

GType gst_omx_base_filter2_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_FILTER2_H */
