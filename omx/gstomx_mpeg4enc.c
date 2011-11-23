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

#include "gstomx_mpeg4enc.h"
#include "gstomx.h"

#include <string.h> /* for memset */

GSTOMX_BOILERPLATE (GstOmxMpeg4Enc, gst_omx_mpeg4enc, GstOmxBaseVideoEnc, GST_OMX_BASE_VIDEOENC_TYPE);

enum
{
    ARG_0,
    ARG_PROFILE,
    ARG_LEVEL,
};

#define DEFAULT_PROFILE OMX_VIDEO_MPEG4ProfileSimple
#define DEFAULT_LEVEL OMX_VIDEO_MPEG4Level5

#define GST_TYPE_OMX_VIDEO_MPEG4PROFILETYPE (gst_omx_video_mpeg4profiletype_get_type ())
static GType
gst_omx_video_mpeg4profiletype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_MPEG4ProfileSimple,  "Simple Profile",   "simple"},
            {0, NULL, NULL },
        };


        type = g_enum_register_static ("GstOmxVideoMPEG4Profile", vals);
    }

    return type;
}

#define GST_TYPE_OMX_VIDEO_MPEG4LEVELTYPE (gst_omx_video_mpeg4leveltype_get_type ())
static GType
gst_omx_video_mpeg4leveltype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_MPEG4Level0,     "Level 0",  "level-0"},
            {OMX_VIDEO_MPEG4Level1,     "Level 1",  "level-1"},
            {OMX_VIDEO_MPEG4Level2,     "Level 2",  "level-2"},
            {OMX_VIDEO_MPEG4Level3,     "Level 3",  "level-3"},
            {OMX_VIDEO_MPEG4Level4,     "Level 4",  "level-4"},
            {OMX_VIDEO_MPEG4Level4a,    "Level 4a", "level-4a"},
            {OMX_VIDEO_MPEG4Level5,     "Level 5",  "level-5"},
            {0, NULL, NULL },
        };

        type = g_enum_register_static ("GstOmxVideoMPEG4Level", vals);
    }

    return type;
}

static GstCaps *
generate_src_template (void)
{
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/mpeg",
                                "width", GST_TYPE_INT_RANGE, 16, 4096,
                                "height", GST_TYPE_INT_RANGE, 16, 4096,
                                "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                                "mpegversion", G_TYPE_INT, 4,
                                "systemstream", G_TYPE_BOOLEAN, FALSE,
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

        details.longname = "OpenMAX IL MPEG-4 video encoder";
        details.klass = "Codec/Encoder/Video";
        details.description = "Encodes video in MPEG-4 format with OpenMAX IL";
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
    GstOmxMpeg4Enc *self;
    GOmxCore *gomx;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_MPEG4ENC (obj);
    gomx = (GOmxCore*) omx_base->gomx;

    switch (prop_id)
    {
        case ARG_PROFILE:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            GOmxCore *gomx;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            tProfileLevel.eProfile = g_value_get_enum (value);
            GST_DEBUG_OBJECT (self, "Profile: param=%d",
                              (gint)tProfileLevel.eProfile);

            error_val = OMX_SetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            break;
        }
        case ARG_LEVEL:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            GOmxCore *gomx;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            tProfileLevel.eLevel = g_value_get_enum (value);
            GST_DEBUG_OBJECT (self, "Level: param=%d",
                              (gint)tProfileLevel.eLevel);

            error_val = OMX_SetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
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
    GstOmxMpeg4Enc *self;
    GstOmxBaseFilter *omx_base;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_MPEG4ENC (obj);

    switch (prop_id)
    {
        case ARG_PROFILE:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            GOmxCore *gomx;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            g_value_set_enum (value, tProfileLevel.eProfile);

            GST_DEBUG_OBJECT (self, "Profile: param=%d",
                              (gint)tProfileLevel.eProfile);

            break;
        }
        case ARG_LEVEL:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            GOmxCore *gomx;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            g_value_set_enum (value, tProfileLevel.eLevel);

            GST_DEBUG_OBJECT (self, "Level: param=%d",
                              (gint)tProfileLevel.eLevel);

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
        g_param_spec_enum ("profile", "MPEG4 Profile",
                    "MPEG4 Profile",
                    GST_TYPE_OMX_VIDEO_MPEG4PROFILETYPE,
                    DEFAULT_PROFILE,
                    G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_LEVEL,
        g_param_spec_enum ("level", "MPEG4 Level",
                    "MPEG4 Level",
                    GST_TYPE_OMX_VIDEO_MPEG4LEVELTYPE,
                    DEFAULT_LEVEL,
                    G_PARAM_READWRITE));
    }
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

        G_OMX_PORT_GET_DEFINITION (omx_base_filter->out_port, &param);

        width = param.format.video.nFrameWidth;
        height = param.format.video.nFrameHeight;
    }

    {
        GstCaps *new_caps;

        new_caps = gst_caps_new_simple ("video/mpeg",
                                        "mpegversion", G_TYPE_INT, 4,
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height,
                                        "framerate", GST_TYPE_FRACTION,
                                        omx_base->framerate_num, omx_base->framerate_denom,
                                        "systemstream", G_TYPE_BOOLEAN, FALSE,
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

    omx_base->compression_format = OMX_VIDEO_CodingMPEG4;

    omx_base_filter->gomx->settings_changed_cb = settings_changed_cb;
}
