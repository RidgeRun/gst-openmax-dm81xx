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

#include "gstomx_h263enc.h"
#include "gstomx.h"

#include <string.h> /* for memset */

GSTOMX_BOILERPLATE (GstOmxH263Enc, gst_omx_h263enc, GstOmxBaseVideoEnc, GST_OMX_BASE_VIDEOENC_TYPE);

enum
{
    ARG_0,
    ARG_PROFILE,
    ARG_LEVEL,
};

#define DEFAULT_PROFILE OMX_VIDEO_H263ProfileBaseline
#define DEFAULT_LEVEL OMX_VIDEO_H263Level40

#define GST_TYPE_OMX_VIDEO_H263PROFILETYPE (gst_omx_video_h263profiletype_get_type ())
static GType
gst_omx_video_h263profiletype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_H263ProfileBaseline,           "Base Profile",        "base"},
            {OMX_VIDEO_H263ProfileH320Coding,         "H.320 Coding",        "h320"},
            {OMX_VIDEO_H263ProfileBackwardCompatible, "Backward Compatible", "backward"},
            {OMX_VIDEO_H263ProfileISWV2,              "ISWV2",               "isw2"},
            {OMX_VIDEO_H263ProfileISWV3,              "ISWV3",               "isw3"},
            {OMX_VIDEO_H263ProfileHighCompression,    "High Compression",    "high-compression"},
            {OMX_VIDEO_H263ProfileInternet,           "Internet",            "internet"},
            {OMX_VIDEO_H263ProfileInterlace,          "Interlace",           "interlace"},
            {OMX_VIDEO_H263ProfileHighLatency,        "High Latency",        "high-latency"},
            {0, NULL, NULL},
        };

        type = g_enum_register_static ("GstOmxVideoH263Profile", vals);
    }

    return type;
}

#define GST_TYPE_OMX_VIDEO_H263LEVELTYPE (gst_omx_video_h263leveltype_get_type ())
static GType
gst_omx_video_h263leveltype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_H263Level10,       "Level 10",        "level-10"},
            {OMX_VIDEO_H263Level20,       "Level 20",        "level-20"},
            {OMX_VIDEO_H263Level30,       "Level 30",        "level-30"},
            {OMX_VIDEO_H263Level40,       "Level 40",        "level-40"},
            {OMX_VIDEO_H263Level45,       "Level 45",        "level-45"},
            {OMX_VIDEO_H263Level50,       "Level 50",        "level-50"},
            {OMX_VIDEO_H263Level60,       "Level 60",        "level-60"},
            {OMX_VIDEO_H263Level70,       "Level 70",        "level-70"},
            {0, NULL, NULL},
        };

        type = g_enum_register_static ("GstOmxVideoH263Level", vals);
    }

    return type;
}

static GstCaps *
generate_src_template (void)
{
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-h263",
                                "variant", G_TYPE_STRING, "itu",
                                "width", GST_TYPE_INT_RANGE, 16, 4096,
                                "height", GST_TYPE_INT_RANGE, 16, 4096,
                                "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
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

        details.longname = "OpenMAX IL H.263 video encoder";
        details.klass = "Codec/Encoder/Video";
        details.description = "Encodes video in H.263 format with OpenMAX IL";
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
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseFilter *omx_base;
    GstOmxH263Enc *self;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_H263ENC (obj);

    switch (prop_id)
    {
        case ARG_PROFILE:
        {
            self->profile = g_value_get_enum (value);
            break;
        }
        case ARG_LEVEL:
        {
            self->level = g_value_get_enum (value);
            break;
        }
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
    GstOmxBaseFilter *omx_base;
    GstOmxH263Enc *self;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_H263ENC (obj);

    switch (prop_id)
    {
        case ARG_PROFILE:
        {
            g_value_set_enum (value, self->profile);
            break;
        }
        case ARG_LEVEL:
        {
            g_value_set_enum (value, self->level);
            break;
        }
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

        g_object_class_install_property (gobject_class, ARG_PROFILE,
                    g_param_spec_enum ("profile", "H.263 Profile",
                    "H.263 Profile",
                    GST_TYPE_OMX_VIDEO_H263PROFILETYPE,
                    DEFAULT_PROFILE,
                    G_PARAM_READWRITE));
        g_object_class_install_property (gobject_class, ARG_LEVEL,
                    g_param_spec_enum ("level", "H.263 Level",
                    "H.263 Level",
                    GST_TYPE_OMX_VIDEO_H263LEVELTYPE,
                    DEFAULT_LEVEL,
                    G_PARAM_READWRITE));
    }
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxH263Enc *self;
    GOmxCore *gomx;

    self = GST_OMX_H263ENC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");
    {
        OMX_VIDEO_PARAM_H263TYPE tParamH263Type;
        OMX_ERRORTYPE error_val = OMX_ErrorNone;
        _G_OMX_INIT_PARAM (&tParamH263Type);
        tParamH263Type.nPortIndex = omx_base->out_port->port_index;
        error_val = OMX_GetParameter (gomx->omx_handle,
                                      OMX_IndexParamVideoH263,
                                      &tParamH263Type);
        g_assert (error_val == OMX_ErrorNone);
        if (self->profile != 0)
            tParamH263Type.eProfile = self->profile;
        else
            tParamH263Type.eProfile = DEFAULT_PROFILE;
        GST_DEBUG_OBJECT (self, "Profile: param=%d",
                          (gint)tParamH263Type.eProfile);
        if (self->level != 0)
            tParamH263Type.eLevel = self->level;
        else
            tParamH263Type.eLevel = DEFAULT_LEVEL;
        GST_DEBUG_OBJECT (self, "Level: param=%d",
                          (gint)tParamH263Type.eLevel);
        error_val = OMX_SetParameter (gomx->omx_handle,
                                      OMX_IndexParamVideoH263,
                                      &tParamH263Type);
        g_assert (error_val == OMX_ErrorNone);
    }
    GST_INFO_OBJECT (omx_base, "end");
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseVideoEnc *omx_base;
    GstOmxBaseFilter *omx_base_filter;
    guint width;
    guint height;

    omx_base_filter = core->object;
    omx_base = GST_OMX_BASE_VIDEOENC (omx_base_filter);

    GST_DEBUG_OBJECT (omx_base, "settings changed");

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        memset (&param, 0, sizeof (param));
        param.nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
        param.nVersion.s.nVersionMajor = 1;
        param.nVersion.s.nVersionMinor = 1;

        param.nPortIndex = 1;
        OMX_GetParameter (core->omx_handle, OMX_IndexParamPortDefinition, &param);

        width = param.format.video.nFrameWidth;
        height = param.format.video.nFrameHeight;
    }

    {
        GstCaps *new_caps;

        new_caps = gst_caps_new_simple ("video/x-h263",
                                        "variant", G_TYPE_STRING, "itu",
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height,
                                        "framerate", GST_TYPE_FRACTION,
                                        omx_base->framerate_num, omx_base->framerate_denom,
                                        NULL);

        GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
        gst_pad_set_caps (omx_base_filter->srcpad, new_caps);
    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base_filter;
    GstOmxBaseVideoEnc *omx_base;

    omx_base_filter = GST_OMX_BASE_FILTER (instance);
    omx_base = GST_OMX_BASE_VIDEOENC (instance);

    omx_base->omx_setup = omx_setup;

    omx_base->compression_format = OMX_VIDEO_CodingH263;

    omx_base_filter->gomx->settings_changed_cb = settings_changed_cb;
}
