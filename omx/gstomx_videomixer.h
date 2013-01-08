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

#ifndef GSTOMX_VIDEOMIXER_H
#define GSTOMX_VIDEOMIXER_H

#include <gst/gst.h>

#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfpc.h>

#include "gstomx_videomixerpad.h"

G_BEGIN_DECLS

#define GST_OMX_VIDEO_MIXER(obj) ((GstOmxVideoMixer *) (obj))
#define GST_OMX_VIDEO_MIXER_TYPE (gst_omx_video_mixer_get_type ())
#define GST_OMX_VIDEO_MIXER_CLASS(obj) ((GstOmxVideoMixerClass *) (obj))
#define GST_OMX_VIDEO_MIXER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_OMX_VIDEO_MIXER_TYPE, GstOmxVideoMixerClass))
#define GST_IS_OMX_VIDEO_MIXER(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_OMX_VIDEO_MIXER_TYPE))
     

typedef struct GstOmxVideoMixer GstOmxVideoMixer;
typedef struct GstOmxVideoMixerClass GstOmxVideoMixerClass;

#include "gstomx_util.h"
#include <async_queue.h>

#define NUM_PORTS 8

typedef struct ip_params {
     guint idx;
     gint in_width, in_height, in_stride;
	 AsyncQueue *queue;
	 gboolean eos;
	 GstBuffer *lastBuf;
	 guint order;
	 guint outWidth;
	 guint outHeight;
	 guint outX;
	 guint outY;
} ip_params ;

typedef struct Olist {
     guint idx;
     gint order;
} Olist ;


struct GstOmxVideoMixer
{
    GstElement element;

    GstVideoMixerPad *sinkpad[NUM_PORTS];
    GstPad *srcpad;

    GOmxCore *gomx;
    GOmxPort *in_port[NUM_PORTS];
    GOmxPort *out_port[NUM_PORTS];

    char *omx_role;
    char *omx_component;
    char *omx_library;
    gboolean ready;
    GMutex *ready_lock;

    GstFlowReturn last_pad_push_return;
    GstBuffer *codec_data;
    GstClockTime duration;
    /* Borrowed from gstomx_vfpc */
	gint framerate_num;
    gint framerate_denom;
    gboolean port_configured;
    GstPadSetCapsFunction sink_setcaps;
   // gint in_width, in_height, in_stride;
    gint out_width, out_height, out_stride;
    gint left, top;
    gint port_index, input_port_index[NUM_PORTS], output_port_index[NUM_PORTS];
	pthread_t input_loop;
	//AsyncQueue *queue[NUM_PORTS];
	gboolean ipCreated;
	gboolean eos;
    //gpointer g_class;
    ip_params chInfo[NUM_PORTS]; 
	guint numEosPending;
	GSem *bufferSem;
	GstClockTime timestamp;
	
	guint settingsChanged;
	Olist **orderList;
	GMutex *loop_lock;
	guint next_sinkpad;
	guint numpads;
	guint outbufsize;
};

struct GstOmxVideoMixerClass
{
    GstElementClass parent_class;

    GstFlowReturn (*push_buffer) (GstOmxVideoMixer *self, GstBuffer *buf);
    GstFlowReturn (*pad_chain) (GstPad *pad, GstBuffer *buf);
    gboolean (*pad_event) (GstPad *pad, GstEvent *event);
};

GType gst_omx_video_mixer_get_type (void);

G_END_DECLS

#endif /* GSTOMX_VIDEOMIXER_H */
