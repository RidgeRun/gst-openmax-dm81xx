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

#include "gstomx_base_videodec.h"
#include "gstomx.h"
#include "gstomx_buffertransport.h"

#include <gst/video/video.h>

#ifdef USE_OMXTICORE
#  include <OMX_TI_Index.h>
#  include <OMX_TI_Common.h>
#endif

#include <string.h> /* for memset */

GSTOMX_BOILERPLATE (GstOmxBaseVideoDec, gst_omx_base_videodec, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);

/* OMX component not handling other color formats properly.. use this workaround
 * until component is fixed or we rebase to get config file support..
 */
#define VIDDEC_COLOR_WORKAROUND
#ifdef VIDDEC_COLOR_WORKAROUND
#  undef GSTOMX_ALL_FORMATS
#  define GSTOMX_ALL_FORMATS  "{NV12}"
#endif

static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE ("src",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (
                        GSTOMX_ALL_FORMATS, "[ 0, max ]"))
        );

static GstFlowReturn push_buffer (GstOmxBaseFilter *self, GstBuffer *buf);

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GST_OMX_BASE_FILTER_CLASS (g_class)->push_buffer = push_buffer;
}

static GstFlowReturn
push_buffer (GstOmxBaseFilter *omx_base, GstBuffer *buf)
{
    GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);
    guint n_offset = omx_base->out_port->n_offset;
    if (n_offset)
    {
		if (self->prev_rowstride != self->rowstride ||
			self->prev_n_stride != n_offset) {
			self->prev_rowstride = self->rowstride;
			self->prev_n_stride = n_offset;

			gst_pad_push_event (omx_base->srcpad,
					gst_event_new_crop (n_offset / self->rowstride, /* top */
						n_offset % self->rowstride, /* left */
						-1, -1)); /* width/height: can be invalid for now */
		}
    }
    return parent_class->push_buffer (omx_base, buf);
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseFilter *omx_base;
    GstOmxBaseVideoDec *self;
    GstCaps *new_caps;

    omx_base = core->object;
    self = GST_OMX_BASE_VIDEODEC (omx_base);

    GST_DEBUG_OBJECT (omx_base, "settings changed");

    new_caps = gst_caps_intersect (gst_pad_get_caps (omx_base->srcpad),
           gst_pad_peer_get_caps (omx_base->srcpad));

    if (!gst_caps_is_fixed (new_caps))
    {
        gst_caps_do_simplify (new_caps);
        GST_INFO_OBJECT (omx_base, "pre-fixated caps: %" GST_PTR_FORMAT, new_caps);
        gst_pad_fixate_caps (omx_base->srcpad, new_caps);
    }

    GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
    GST_INFO_OBJECT (omx_base, "old caps are: %" GST_PTR_FORMAT, GST_PAD_CAPS (omx_base->srcpad));

    gst_pad_set_caps (omx_base->srcpad, new_caps);
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxBaseVideoDec *self;
    GstOmxBaseFilter *omx_base;
    GOmxCore *gomx;

    gint width = 0;
    gint height = 0;

    self = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    structure = gst_caps_get_structure (caps, 0);

    g_return_val_if_fail (structure, FALSE);

    if (!(gst_structure_get_int (structure, "width", &width) &&
            gst_structure_get_int (structure, "height", &height)))
    {
        GST_WARNING_OBJECT (self, "width and/or height not set in caps: %dx%d",
                width, height);
        return FALSE;
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

    {
        const GValue *codec_data;
        GstBuffer *buffer;

        codec_data = gst_structure_get_value (structure, "codec_data");
        if (codec_data)
        {
            buffer = gst_value_get_buffer (codec_data);
            omx_base->codec_data = buffer;
            gst_buffer_ref (buffer);
        }
    }

    /* REVISIT: to use OMX package from EZSDK you need to configure ports  */
    #ifdef USE_OMXTICORE
    {
        if (self->initialize_port) {
            self->extendedParams.width = width;
            self->extendedParams.height = height;
            self->initialize_port(omx_base);
        }
    }
    #endif

    /* Input port configuration. */
    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

        param.format.video.nFrameWidth = width;
        param.format.video.nFrameHeight = height;
        param.nBufferSize = width * height;

        G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
        GST_DEBUG_OBJECT (self, "G_OMX_PORT_SET_DEFINITION");
    }

    self->inport_configured = TRUE;

    if (self->sink_setcaps)
        self->sink_setcaps (pad, caps);

    return gst_pad_set_caps (pad, caps);
}

static GstCaps *
src_getcaps (GstPad *pad)
{
    GstCaps *caps;
    GstOmxBaseVideoDec *self   = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
    GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (self);

    if (omx_base->gomx->omx_state > OMX_StateLoaded)
    {
        /* currently, we cannot change caps once out of loaded..  later this
         * could possibly be supported by enabling/disabling the port..
         */
        GST_DEBUG_OBJECT (self, "cannot getcaps in %d state", omx_base->gomx->omx_state);
        return GST_PAD_CAPS (pad);
    }

    if (self->inport_configured)
    {
        /* if we already have src-caps, we want to take the already configured
         * width/height/etc.  But we can still support any option of rowstride,
         * so we still don't want to return fixed caps
         */
        OMX_PARAM_PORTDEFINITIONTYPE param;
        int i;

        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

        caps = gst_caps_new_empty ();

        for (i=1; i<2; i++)
        {
            GstStructure *struc = gst_structure_new (
                    (i ? "video/x-raw-yuv-strided" : "video/x-raw-yuv"),
                    "width",  G_TYPE_INT, param.format.video.nFrameWidth,
                    "height", G_TYPE_INT, param.format.video.nFrameHeight,
#ifdef VIDDEC_COLOR_WORKAROUND
                    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('N', 'V', '1', '2'),
#endif
                    NULL);

            if(i)
            {
                /* if buffer sharing is used, we let the upstream that allocates
                 * the buffer dictate stride, otherwise we let the OMX component
                 * decide on the stride
                 */
                if (omx_base->out_port->share_buffer)
                {
                    gst_structure_set (struc,
                            "rowstride", GST_TYPE_INT_RANGE, 1, G_MAXINT,
                            NULL);
                }
                else
                {
					gst_structure_set (struc,
							"rowstride", G_TYPE_INT, param.format.video.nStride,
							NULL);
                }
            }

            if (self->framerate_denom)
            {
                gst_structure_set (struc,
                        "framerate", GST_TYPE_FRACTION, self->framerate_num, self->framerate_denom,
                        NULL);
            }

            if (self->pixel_aspect_ratio_denom)
            {
                gst_structure_set (struc,
                        "pixel-aspect-ratio", GST_TYPE_FRACTION, self->pixel_aspect_ratio_num, 
						self->pixel_aspect_ratio_denom, NULL);
            }

			gst_structure_set (struc,
				"interlaced", G_TYPE_BOOLEAN, self->interlaced, NULL);

            gst_caps_append_structure (caps, struc);
        }
    }
    else
    {
        /* we don't have valid width/height/etc yet, so just use the template.. */
        caps = gst_static_pad_template_get_caps (&src_template);
        GST_DEBUG_OBJECT (self, "caps=%"GST_PTR_FORMAT, caps);
    }

#ifndef VIDDEC_COLOR_WORKAROUND
    caps = g_omx_port_set_video_formats (omx_base->out_port, caps);
#endif

    GST_DEBUG_OBJECT (self, "caps=%"GST_PTR_FORMAT, caps);

    return caps;
}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxBaseVideoDec *self;
    GstOmxBaseFilter *omx_base;

    GstVideoFormat format;
    gint width, height, rowstride;

    self = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    GST_INFO_OBJECT (omx_base, "setcaps (src): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    if (gst_video_format_parse_caps_strided (caps,
            &format, &width, &height, &rowstride))
    {
        /* Output port configuration: */
        OMX_PARAM_PORTDEFINITIONTYPE param;

        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

        /* REVISIT: if rowstride is not defined then use the one from output params */
        if (!rowstride)
            rowstride = param.format.video.nStride;

        param.format.video.eColorFormat = g_omx_fourcc_to_colorformat (
                gst_video_format_to_fourcc (format));
        param.format.video.nFrameWidth  = width;
        param.format.video.nFrameHeight = height;
        param.format.video.nStride      = self->rowstride = rowstride;

        G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &param);
        GST_INFO_OBJECT (omx_base,"G_OMX_PORT_SET_DEFINITION");
    }

    /* save the src caps later needed by omx transport buffer */
    if (omx_base->out_port->caps)
        gst_caps_unref (omx_base->out_port->caps);
    omx_base->out_port->caps = gst_caps_copy (caps);

    return TRUE;
}

#if 0
static gboolean
src_query (GstPad *pad, GstQuery *query)
{
    GstOmxBaseVideoDec *self   = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
    GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (self);
    gboolean ret = FALSE;

    GST_DEBUG_OBJECT (self, "begin");

    if (GST_QUERY_TYPE (query) == GST_QUERY_BUFFERS) {
        const GstCaps *caps;
        OMX_ERRORTYPE err;
        OMX_PARAM_PORTDEFINITIONTYPE param;

        _G_OMX_INIT_PARAM (&param);

        gst_query_parse_buffers_caps (query, &caps);

        /* ensure the caps we are querying are the current ones, otherwise
         * results are meaningless..
         *
         * @todo should we save and restore current caps??
         */
        src_setcaps (pad, (GstCaps *)caps);

        param.nPortIndex = omx_base->out_port->port_index;
        err = OMX_GetParameter (omx_base->gomx->omx_handle,
                OMX_IndexParamPortDefinition, &param);
        g_assert (err == OMX_ErrorNone);

        param.nBufferCountActual = param.nBufferCountMin;
        err = OMX_SetParameter (omx_base->gomx->omx_handle,
                OMX_IndexParamPortDefinition, &param);
        g_assert (err == OMX_ErrorNone);

        GST_DEBUG_OBJECT (self, "min buffers: %ld", param.nBufferCountMin);

        gst_query_set_buffers_count (query, param.nBufferCountMin);

/* REVISIT: OMX_TI_IndexParam2DBufferAllocDimension is not implemented in EZSDK OMX components */
#if 0
#ifdef USE_OMXTICORE
        {
            OMX_CONFIG_RECTTYPE rect;
            _G_OMX_INIT_PARAM (&rect);

            rect.nPortIndex = omx_base->out_port->port_index;
            err = OMX_GetParameter (omx_base->gomx->omx_handle,
                    OMX_TI_IndexParam2DBufferAllocDimension, &rect);
            if (err == OMX_ErrorNone) {
                GST_DEBUG_OBJECT (self, "min dimensions: %ldx%ld",
                        rect.nWidth, rect.nHeight);
                gst_query_set_buffers_dimensions (query,
                        rect.nWidth, rect.nHeight);
            }
        }
#endif
#endif

        ret = TRUE;
    }

    GST_DEBUG_OBJECT (self, "end -> %d", ret);

    return ret;
}
#endif

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVideoDec *self;
    GOmxCore *gomx;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        /* Input port configuration. */
        G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

        param.format.video.eCompressionFormat = self->compression_format;

        G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
        GST_DEBUG_OBJECT (self, "G_OMX_PORT_SET_DEFINITION 1!!!");
    }

    GST_INFO_OBJECT (omx_base, "end");
}

static void push_cb (GstOmxBaseFilter *omx_base, GstBuffer *buf)
{
    GstOmxBaseVideoDec *self;
	OMX_BUFFERHEADERTYPE *omxbuffer;
	GstCaps *caps, tmp;
	GstStructure *structure;
	gboolean i;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
	omxbuffer = GST_GET_OMXBUFFER(buf);

	/* Change interlaced flag in srcpad caps if decoder differs with 
	   what is already got from the upstream element */
	i = (0 != (omxbuffer->nFlags & 
				OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE));
	if (i != self->interlaced) {
		caps = gst_caps_copy(GST_PAD_CAPS(omx_base->srcpad));
		self->interlaced = i;
		structure = gst_caps_get_structure (caps, 0);
		if (structure) {
			gst_structure_set (structure,
					"interlaced", G_TYPE_BOOLEAN, self->interlaced, NULL);
		}
		gst_pad_set_caps(omx_base->srcpad, caps);
		if (GST_BUFFER_CAPS(buf)) gst_caps_unref(GST_BUFFER_CAPS(buf));
		GST_BUFFER_CAPS(buf) = caps;
	}
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;
    GstOmxBaseVideoDec *self;

    omx_base = GST_OMX_BASE_FILTER (instance);
    self = GST_OMX_BASE_VIDEODEC (omx_base);

    omx_base->omx_setup = omx_setup;
    omx_base->push_cb = push_cb;

    omx_base->gomx->settings_changed_cb = settings_changed_cb;

    omx_base->in_port->omx_allocate = TRUE;
    omx_base->out_port->omx_allocate = TRUE;
    omx_base->in_port->share_buffer = FALSE;
    omx_base->in_port->always_copy  = TRUE;
    omx_base->out_port->share_buffer = FALSE;
    omx_base->out_port->always_copy = FALSE;
    omx_base->filterType = FILTER_DECODER;
    gst_pad_set_setcaps_function (omx_base->sinkpad,
            GST_DEBUG_FUNCPTR (sink_setcaps));

    gst_pad_set_getcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_getcaps));
    gst_pad_set_setcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_setcaps));
//    gst_pad_set_query_function (omx_base->srcpad,
//            GST_DEBUG_FUNCPTR (src_query));

	self->prev_rowstride = 0;
	self->prev_n_stride = 0;
}

