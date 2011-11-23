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

#include "gstomx_mp3dec.h"
#include "gstomx.h"

#include <string.h> /* for memset */

#ifdef USE_OMXTIAUDIODEC
#  include <TIDspOmx.h>
#endif
#include <OMX_Core.h>

enum
{
    ARG_0,
    ARG_FRAMEMODE,
};

#define DEFAULT_FRAMEMODE FALSE

GSTOMX_BOILERPLATE (GstOmxMp3Dec, gst_omx_mp3dec, GstOmxBaseAudioDec, GST_OMX_BASE_AUDIODEC_TYPE);

static GstCaps *
generate_src_template (void)
{
    GstCaps *caps;

    caps = gst_caps_new_simple ("audio/x-raw-int",
                                "endianness", G_TYPE_INT, G_BYTE_ORDER,
                                "width", G_TYPE_INT, 16,
                                "depth", G_TYPE_INT, 16,
                                "rate", GST_TYPE_INT_RANGE, 8000, 96000,
                                "signed", G_TYPE_BOOLEAN, TRUE,
                                "channels", GST_TYPE_INT_RANGE, 1, 2,
                                NULL);

    return caps;
}

static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;

    caps = gst_caps_new_simple ("audio/mpeg",
                                "mpegversion", G_TYPE_INT, 1,
                                "layer", G_TYPE_INT, 3,
                                "rate", GST_TYPE_INT_RANGE, 8000, 48000,
                                "channels", GST_TYPE_INT_RANGE, 1, 8,
                                "parsed", G_TYPE_BOOLEAN, TRUE,
                                NULL);

    return caps;
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL MP3 audio decoder";
        details.klass = "Codec/Decoder/Audio";
        details.description = "Decodes audio in MP3 format with OpenMAX IL";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    {
        GstPadTemplate *template;

        template = gst_pad_template_new ("src", GST_PAD_SRC,
                                         GST_PAD_ALWAYS,
                                         generate_src_template ());

        gst_element_class_add_pad_template (element_class, template);
    }

    {
        GstPadTemplate *template;

        template = gst_pad_template_new ("sink", GST_PAD_SINK,
                                         GST_PAD_ALWAYS,
                                         generate_sink_template ());

        gst_element_class_add_pad_template (element_class, template);
    }
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxMp3Dec *self;

    self = GST_OMX_MP3DEC (obj);

    switch (prop_id)
    {
        case ARG_FRAMEMODE:
            self->framemode = g_value_get_boolean  (value);
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
    GstOmxMp3Dec *self;

    self = GST_OMX_MP3DEC (obj);

    switch (prop_id)
    {
        case ARG_FRAMEMODE:
            g_value_set_boolean (value, self->framemode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    /* This is specific for TI. */
#ifdef USE_OMXTIAUDIODEC
    GOmxCore *gomx = omx_base->gomx;
    GstOmxMp3Dec *self;
    self = GST_OMX_MP3DEC (omx_base);
    {
        OMX_INDEXTYPE index;
        TI_OMX_DSP_DEFINITION audioinfo;

        memset (&audioinfo, 0, sizeof (audioinfo));

        audioinfo.framemode = self->framemode;

        g_assert( OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.index.config.mp3headerinfo",
                &index) == OMX_ErrorNone);

        g_assert( OMX_SetConfig (gomx->omx_handle, index, &audioinfo)== OMX_ErrorNone);

        GST_DEBUG_OBJECT (omx_base, "OMX_SetConfig OMX.TI.index.config.mp3headerinfo");
        GST_DEBUG_OBJECT (omx_base, "setting frame-mode");
    }
#endif

    /* Output parameter configuration. */
    {
        OMX_AUDIO_PARAM_PCMMODETYPE param;
        GstOmxBaseAudioDec *base_audiodec;
        base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);

        G_OMX_PORT_GET_PARAM (omx_base->out_port, OMX_IndexParamAudioPcm, &param);

        param.nSamplingRate = base_audiodec->rate;
        param.nChannels = base_audiodec->channels;

        G_OMX_PORT_SET_PARAM (omx_base->out_port, OMX_IndexParamAudioPcm, &param);
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

        g_object_class_install_property (gobject_class, ARG_FRAMEMODE,
                                          g_param_spec_boolean ("framemode", "Frame mode",
                                                            "Frame mode", DEFAULT_FRAMEMODE, G_PARAM_READWRITE));
    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
	GstOmxBaseFilter *omx_base;
	GstOmxMp3Dec *self;

	self = GST_OMX_MP3DEC (instance);

	omx_base = GST_OMX_BASE_FILTER (instance);

	GST_DEBUG_OBJECT (omx_base, "start");

	omx_base->omx_setup = omx_setup;

	self->framemode = DEFAULT_FRAMEMODE;

}
