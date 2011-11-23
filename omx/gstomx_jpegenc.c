/*
 * Copyright (C) 2008-2009 Nokia Corporation.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "gstomx_jpegenc.h"
#include "gstomx_base_filter.h"
#include "gstomx.h"

#include <gst/video/video.h>
#include <string.h>
#include <stdlib.h>

enum
{
    ARG_0,
    ARG_QUALITY,
};

#define DEFAULT_QUALITY 90

GSTOMX_BOILERPLATE (GstOmxJpegEnc, gst_omx_jpegenc, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);

static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE ("src",
                GST_PAD_SRC, GST_PAD_ALWAYS,
                GST_STATIC_CAPS ("image/jpeg, "
                        "width = (int)[16,4096], "
                        "height = (int)[16,4096], "
                        "framerate = (fraction)[0/1,max];")
        );


static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE ("sink",
                GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV (
                        GSTOMX_ALL_FORMATS))
        );

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL JPEG image encoder";
        details.klass = "Codec/Encoder/Image";
        details.description = "Encodes image in JPEG format with OpenMAX IL";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template));
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxJpegEnc *self;

    self = GST_OMX_JPEGENC (obj);

    switch (prop_id)
    {
        case ARG_QUALITY:
            self->quality = g_value_get_uint (value);
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
    GstOmxJpegEnc *self;

    self = GST_OMX_JPEGENC (obj);

    switch (prop_id)
    {
        case ARG_QUALITY:
            g_value_set_uint (value, self->quality);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (g_class);

    /* Properties stuff */
    {
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;

        g_object_class_install_property (gobject_class, ARG_QUALITY,
                                         g_param_spec_uint ("quality", "Quality of image",
                                                            "Set the quality from 0 to 100",
                                                            0, 100, DEFAULT_QUALITY, G_PARAM_READWRITE));
    }
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseFilter *omx_base;
    GstOmxJpegEnc *self;
    guint width;
    guint height;

    omx_base = core->object;
    self = GST_OMX_JPEGENC (omx_base);

    GST_DEBUG_OBJECT (omx_base, "settings changed");

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

        width = param.format.image.nFrameWidth;
        height = param.format.image.nFrameHeight;
    }

    {
        GstCaps *new_caps;

        new_caps = gst_caps_new_simple ("image/jpeg",
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height,
                                        "framerate", GST_TYPE_FRACTION,
                                        self->framerate_num, self->framerate_denom,
                                        NULL);

        GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
        gst_pad_set_caps (omx_base->srcpad, new_caps);
    }
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxBaseFilter *omx_base;
    GstOmxJpegEnc *self;
    GOmxCore *gomx;
    OMX_COLOR_FORMATTYPE color_format = OMX_COLOR_FormatYUV420PackedPlanar;
    gint width = 0;
    gint height = 0;

    omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    self = GST_OMX_JPEGENC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

    structure = gst_caps_get_structure (caps, 0);

    gst_structure_get_int (structure, "width", &width);
    gst_structure_get_int (structure, "height", &height);

    if (!gst_structure_get_fraction (structure, "framerate",
                                     &self->framerate_num,
                                     &self->framerate_denom))
    {
        self->framerate_num = 0;
        self->framerate_denom = 1;
        omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,0,1);
        GST_DEBUG_OBJECT (self, "Nominal frame duration =%"GST_TIME_FORMAT,
                            GST_TIME_ARGS (omx_base->duration));
    }

    if (strcmp (gst_structure_get_name (structure), "video/x-raw-yuv") == 0)
    {
        guint32 fourcc;

        if (gst_structure_get_fourcc (structure, "format", &fourcc))
        {
            color_format = g_omx_fourcc_to_colorformat (fourcc);
        }
    }

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        /* Input port configuration. */
        G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

        param.format.image.nFrameWidth = width;
        param.format.image.nFrameHeight = height;
        param.format.image.eColorFormat = color_format;

        G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
    }

    return gst_pad_set_caps (pad, caps);
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxJpegEnc *self;
    GOmxCore *gomx;

    self = GST_OMX_JPEGENC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        /* Output port configuration. */
        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

        param.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;

        G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &param);

        /* some workarounds required for TI components. */
        {
            guint32 fourcc;
            gint width, height;

            /* the component should do this instead */
            {
                G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

                width = param.format.image.nFrameWidth;
                height = param.format.image.nFrameHeight;
                fourcc = g_omx_colorformat_to_fourcc (
                        param.format.image.eColorFormat);

                /* this is against the standard; nBufferSize is read-only. */
                param.nBufferSize = gst_video_format_get_size (
                        gst_video_format_from_fourcc (fourcc),
                        GST_ROUND_UP_16 (width), GST_ROUND_UP_16 (height));

                G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
            }

            /* the component should do this instead */
            {
                G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

                param.nBufferSize = width * height;

                param.format.image.nFrameWidth = width;
                param.format.image.nFrameHeight = height;

                G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &param);
            }
        }
    }

    {
        OMX_IMAGE_PARAM_QFACTORTYPE param;

        G_OMX_PORT_GET_PARAM (omx_base->out_port, OMX_IndexParamQFactor, &param);

        param.nQFactor = self->quality;

        G_OMX_PORT_SET_PARAM (omx_base->out_port, OMX_IndexParamQFactor, &param);
    }

    GST_INFO_OBJECT (omx_base, "end");
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;
    GstOmxJpegEnc *self;

    omx_base = GST_OMX_BASE_FILTER (instance);
    self = GST_OMX_JPEGENC (instance);

    omx_base->omx_setup = omx_setup;

    omx_base->gomx->settings_changed_cb = settings_changed_cb;

    gst_pad_set_setcaps_function (omx_base->sinkpad, sink_setcaps);

    self->framerate_num = 0;
    self->framerate_denom = 1;
    self->quality = DEFAULT_QUALITY;
}
