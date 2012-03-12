/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 prashant <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-omxbufferalloc
 *
 * FIXME:Describe omxbufferalloc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omxbufferalloc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstomx.h"
#include "gstomx_interface.h"

#include "gstomxbufferalloc.h"


GST_DEBUG_CATEGORY_STATIC (gst_omx_buffer_alloc_debug);
//#define GST_CAT_DEFAULT gst_omx_buffer_alloc_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_NUMBUFFERS
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstomxBufferAlloc, gst_omx_buffer_alloc, GstElement,
    GST_TYPE_ELEMENT);

static void gst_omx_buffer_alloc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_buffer_alloc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_omx_buffer_alloc_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_omx_buffer_alloc_chain (GstPad * pad, GstBuffer * buf);

GstFlowReturn 
gst_omx_buffer_alloc_bufferalloc (GstPad *pad, guint64 offset, guint size,
                                      GstCaps *caps, GstBuffer **buf);

static GstStateChangeReturn
gst_omx_buffer_alloc_change_state (GstElement *element,
              GstStateChange transition);



/* GObject vmethod implementations */

static void
gst_omx_buffer_alloc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "omxBufferAlloc",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "prashant <<user@hostname.org>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the omxbufferalloc's class */
static void
gst_omx_buffer_alloc_class_init (GstomxBufferAllocClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_omx_buffer_alloc_set_property;
  gobject_class->get_property = gst_omx_buffer_alloc_get_property;
  gstelement_class->change_state = gst_omx_buffer_alloc_change_state;

  g_object_class_install_property (gobject_class, PROP_NUMBUFFERS,
      g_param_spec_uint ("numBuffers", "number of buffers",
          "Number of buffers to be allocated by component",
              1, 16, 10, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_omx_buffer_alloc_init (GstomxBufferAlloc * filter,
    GstomxBufferAllocClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_chain));

  gst_pad_set_bufferalloc_function(filter->sinkpad,GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_bufferalloc));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
  filter->out_port.num_buffers = 10;
  filter->out_port.always_copy = FALSE;
  filter->cnt = 0;
  filter->out_port.buffers = NULL;
}

static void
gst_omx_buffer_alloc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstomxBufferAlloc *filter = GST_OMXBUFFERALLOC (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
	case PROP_NUMBUFFERS:
	  filter->out_port.num_buffers = g_value_get_uint (value);
	  break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_buffer_alloc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstomxBufferAlloc *filter = GST_OMXBUFFERALLOC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_omx_buffer_alloc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstomxBufferAlloc *filter;
  GstPad *otherpad;

  filter = GST_OMXBUFFERALLOC (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_omx_buffer_alloc_chain (GstPad * pad, GstBuffer * buf)
{
  GstomxBufferAlloc *filter;
  
  filter = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));

  buf = gst_omxbuffertransport_clone (buf, &(filter->out_port));

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
}

void 
gst_omx_buffer_alloc_allocate_buffers (GstomxBufferAlloc *filter, guint size)
{
  guint ii;
  guint numBufs;

  numBufs = filter->out_port.num_buffers;
  printf("allocating %d buffers of size:%d!!\n",numBufs,size);
  filter->out_port.buffers = g_new0 (OMX_BUFFERHEADERTYPE *, numBufs);
  filter->heap = SharedRegion_getHeap(2);
  
  for(ii = 0; ii < numBufs; ii++) {
  	 filter->out_port.buffers[ii] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
     filter->out_port.buffers[ii]->pBuffer = Memory_alloc (filter->heap, size, 128, NULL);
	 printf("allocated outbuf:%p\n",filter->out_port.buffers[ii]->pBuffer);
  }
  filter->allocSize = size;

  return;
}

GstFlowReturn 
gst_omx_buffer_alloc_bufferalloc (GstPad *pad, guint64 offset, guint size,
                                      GstCaps *caps, GstBuffer **buf)
{
  GstomxBufferAlloc *filter;
  filter = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));
  if(filter->out_port.buffers == NULL)
  	gst_omx_buffer_alloc_allocate_buffers (filter,size);

  *buf = gst_buffer_new();
  GST_BUFFER_DATA(*buf) = filter->out_port.buffers[filter->cnt++]->pBuffer;
  GST_BUFFER_SIZE(*buf) = size;
  GST_BUFFER_CAPS(*buf) = caps;
  return;
}

static GstStateChangeReturn
gst_omx_buffer_alloc_change_state (GstElement *element,
              GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstomxBufferAlloc *filter = GST_OMXBUFFERALLOC (element);
	guint ii;
    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
            OMX_Init ();
            break;

        default:
            break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        goto leave;

    switch (transition)
    {
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
			if(filter->out_port.buffers) {
			  for(ii = 0; ii < filter->out_port.num_buffers; ii++) {
    			  Memory_free(filter->heap,filter->out_port.buffers[ii]->pBuffer,filter->allocSize);
			  }
			  g_free(filter->out_port.buffers);
			}
			OMX_Deinit();
            break;

        default:
            break;
    }

leave:
    return ret;
}




