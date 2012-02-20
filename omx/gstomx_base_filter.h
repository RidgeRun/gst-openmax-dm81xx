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

#ifndef GSTOMX_BASE_FILTER_H
#define GSTOMX_BASE_FILTER_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_FILTER(obj) ((GstOmxBaseFilter *) (obj))
#define GST_OMX_BASE_FILTER_TYPE (gst_omx_base_filter_get_type ())
#define GST_OMX_BASE_FILTER_CLASS(obj) ((GstOmxBaseFilterClass *) (obj))
#define GST_OMX_BASE_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_OMX_BASE_FILTER_TYPE, GstOmxBaseFilterClass))

typedef struct GstOmxBaseFilter GstOmxBaseFilter;
typedef struct GstOmxBaseFilterClass GstOmxBaseFilterClass;
typedef void (*GstOmxBaseFilterCb) (GstOmxBaseFilter *self);
typedef void (*GstOmxBaseFilterPushCb) (GstOmxBaseFilter *self, GstBuffer *buf);

#include "gstomx_util.h"
#include <async_queue.h>

typedef enum {
  FILTER_NONE,
  FILTER_DECODER,
  FILTER_ENCODER,
  FILTER_PP
} filtertype;

struct GstOmxBaseFilter
{
    GstElement element;

    GstPad *sinkpad;
    GstPad *srcpad;

    GOmxCore *gomx;
    GOmxPort *in_port;
    GOmxPort *out_port;

    char *omx_role;
    char *omx_component;
    char *omx_library;
    gboolean ready;
    GMutex *ready_lock;

    GstOmxBaseFilterCb omx_setup;
    GstOmxBaseFilterPushCb push_cb;
    GstFlowReturn last_pad_push_return;
    GstBuffer *codec_data;
    GstClockTime duration;
	gboolean isFlushed;
	guint filterType;

};

struct GstOmxBaseFilterClass
{
    GstElementClass parent_class;

    GstFlowReturn (*push_buffer) (GstOmxBaseFilter *self, GstBuffer *buf);
    GstFlowReturn (*pad_chain) (GstPad *pad, GstBuffer *buf);
    gboolean (*pad_event) (GstPad *pad, GstEvent *event);
};

GType gst_omx_base_filter_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_FILTER_H */
