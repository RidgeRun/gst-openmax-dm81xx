/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * Modified by: David Soto <david.soto@ridgerun.com>
 * Copyright (C) 2011 RidgeRun
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

#include "gstomx_h264enc.h"
#include "gstomx.h"

#include <string.h> /* for memset */

GSTOMX_BOILERPLATE (GstOmxH264Enc, gst_omx_h264enc, GstOmxBaseVideoEnc, GST_OMX_BASE_VIDEOENC_TYPE);

enum
{
    ARG_0,
    ARG_BYTESTREAM,
    ARG_PROFILE,
    ARG_LEVEL,
    ARG_I_PERIOD,
    ARG_IDR_PERIOD,
    ARG_FORCE_IDR,
};

#define DEFAULT_BYTESTREAM FALSE
#define DEFAULT_PROFILE OMX_VIDEO_AVCProfileHigh
#define DEFAULT_LEVEL OMX_VIDEO_AVCLevel4

#define GST_TYPE_OMX_VIDEO_AVCPROFILETYPE (gst_omx_video_avcprofiletype_get_type ())
static GType
gst_omx_video_avcprofiletype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_AVCProfileBaseline,       "Base Profile",          "base"},
            {OMX_VIDEO_AVCProfileMain,           "Main Profile",          "main"},
            {OMX_VIDEO_AVCProfileExtended,       "Extended Profile",      "extended"},
            {OMX_VIDEO_AVCProfileHigh,           "High Profile",          "high"},
            {OMX_VIDEO_AVCProfileHigh10,         "High 10 Profile",       "high-10"},
            {OMX_VIDEO_AVCProfileHigh422,        "High 4:2:2 Profile",    "high-422"},
            {OMX_VIDEO_AVCProfileHigh444,        "High 4:4:4 Profile",    "high-444"},
            {0, NULL, NULL },
        };

        type = g_enum_register_static ("GstOmxVideoAVCProfile", vals);
    }

    return type;
}

#define GST_TYPE_OMX_VIDEO_AVCLEVELTYPE (gst_omx_video_avcleveltype_get_type ())
static GType
gst_omx_video_avcleveltype_get_type ()
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue vals[] =
        {
            {OMX_VIDEO_AVCLevel1,        "Level 1",         "level-1"},
            {OMX_VIDEO_AVCLevel1b,       "Level 1b",        "level-1b"},
            {OMX_VIDEO_AVCLevel11,       "Level 11",        "level-11"},
            {OMX_VIDEO_AVCLevel12,       "Level 12",        "level-12"},
            {OMX_VIDEO_AVCLevel13,       "Level 13",        "level-13"},
            {OMX_VIDEO_AVCLevel2,        "Level 2",         "level-2"},
            {OMX_VIDEO_AVCLevel21,       "Level 21",        "level-21"},
            {OMX_VIDEO_AVCLevel22,       "Level 22",        "level-22"},
            {OMX_VIDEO_AVCLevel3,        "Level 3",         "level-3"},
            {OMX_VIDEO_AVCLevel31,       "Level 31",        "level-31"},
            {OMX_VIDEO_AVCLevel32,       "Level 32",        "level-32"},
            {OMX_VIDEO_AVCLevel4,        "Level 4",         "level-4"},
            {OMX_VIDEO_AVCLevel41,       "Level 41",        "level-41"},
            {OMX_VIDEO_AVCLevel42,       "Level 42",        "level-42"},
            {OMX_VIDEO_AVCLevel5,        "Level 5",         "level-5"},
            {OMX_VIDEO_AVCLevel51,       "Level 51",        "level-51"},
            {0, NULL, NULL },
        };

        type = g_enum_register_static ("GstOmxVideoAVCLevel", vals);
    }

    return type;
}

static GstCaps *
generate_src_template (void)
{
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-h264",
                                "width", GST_TYPE_INT_RANGE, 16, 4096,
                                "height", GST_TYPE_INT_RANGE, 16, 4096,
                                "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "alignment", G_TYPE_STRING, "au",
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

        details.longname = "OpenMAX IL H.264/AVC video encoder";
        details.klass = "Codec/Encoder/Video";
        details.description = "Encodes video in H.264/AVC format with OpenMAX IL";
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
    GstOmxH264Enc *self;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_H264ENC (obj);

    switch (prop_id)
    {
        case ARG_BYTESTREAM:
            self->bytestream = g_value_get_boolean (value);
            break;
        case ARG_PROFILE:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            tProfileLevel.eProfile = g_value_get_enum (value);
            GST_DEBUG_OBJECT (self, "Profile: param=%d",
                                    (gint)tProfileLevel.eProfile);

            error_val = OMX_SetParameter (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            break;
        }
        case ARG_LEVEL:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            tProfileLevel.eLevel = g_value_get_enum (value);
            GST_DEBUG_OBJECT (self, "Level: param=%d",
                                    (gint)tProfileLevel.eLevel);

            error_val = OMX_SetParameter (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            break;
        }
       case ARG_I_PERIOD:
        {
            OMX_VIDEO_CONFIG_AVCINTRAPERIOD avcIntraPeriod;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            _G_OMX_INIT_PARAM (&avcIntraPeriod);
            avcIntraPeriod.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetConfig (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexConfigVideoAVCIntraPeriod,
                                          (OMX_PTR)&avcIntraPeriod);
            g_assert (error_val == OMX_ErrorNone);
            avcIntraPeriod.nPFrames = g_value_get_uint (value);

            if(value>0)
            {
            error_val = OMX_SetConfig (g_omx_core_get_handle (omx_base->gomx),
                                          OMX_IndexConfigVideoAVCIntraPeriod,
                                          &avcIntraPeriod);
            }
            g_assert (error_val == OMX_ErrorNone);
            break;
        }
       case ARG_IDR_PERIOD:
        {
            self->idr_period = g_value_get_uint (value);
            break;
        }
       case ARG_FORCE_IDR:
        {
            self->force_idr = g_value_get_boolean (value);
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
    GstOmxH264Enc *self;
    GstOmxBaseFilter *omx_base;

    omx_base = GST_OMX_BASE_FILTER (obj);
    self = GST_OMX_H264ENC (obj);

    switch (prop_id)
    {
        case ARG_BYTESTREAM:
            g_value_set_boolean (value, self->bytestream);
            break;
        case ARG_PROFILE:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE tProfileLevel;
            GOmxCore *gomx;
            OMX_ERRORTYPE error_val = OMX_ErrorNone;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&tProfileLevel);
            tProfileLevel.nPortIndex = omx_base->out_port->port_index;
            error_val = OMX_GetParameter (gomx->omx_handle,
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
            error_val = OMX_GetParameter (gomx->omx_handle,
                                          OMX_IndexParamVideoProfileLevelCurrent,
                                          &tProfileLevel);
            g_assert (error_val == OMX_ErrorNone);
            g_value_set_enum (value, tProfileLevel.eLevel);

            GST_DEBUG_OBJECT (self, "Level: param=%d",
                                    (gint)tProfileLevel.eLevel);

            break;
        }
        case ARG_I_PERIOD:
        {
            OMX_VIDEO_CONFIG_AVCINTRAPERIOD avcIntraPeriod;
            GOmxCore *gomx;

            gomx = (GOmxCore *) omx_base->gomx;
            _G_OMX_INIT_PARAM (&avcIntraPeriod);
            avcIntraPeriod.nPortIndex = omx_base->out_port->port_index;
            OMX_GetConfig (gomx->omx_handle,
                              OMX_IndexConfigVideoAVCIntraPeriod,
                              &avcIntraPeriod);
            g_value_set_uint (value, (gint)avcIntraPeriod.nPFrames);

            break;
        }
        case ARG_IDR_PERIOD:
        {
            g_value_set_uint (value, self->idr_period);

            break;
        }
        case ARG_FORCE_IDR:
        {
            g_value_set_boolean (value, self->force_idr);

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

        g_object_class_install_property (gobject_class, ARG_BYTESTREAM,
                                         g_param_spec_boolean ("bytestream", "BYTESTREAM", "bytestream",
                                                               DEFAULT_BYTESTREAM, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, ARG_PROFILE,
		    g_param_spec_enum ("profile", "H.264 Profile",
                    "H.264 Profile",
                    GST_TYPE_OMX_VIDEO_AVCPROFILETYPE,
                    DEFAULT_PROFILE,
                    G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, ARG_LEVEL,
		    g_param_spec_enum ("level", "H.264 Level",
                    "H.264 Level",
                    GST_TYPE_OMX_VIDEO_AVCLEVELTYPE,
                    DEFAULT_LEVEL,
                    G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_I_PERIOD,
            g_param_spec_uint ("i-period", "Specifies periodicity of I frames",
                    "Specifies periodicity of I frames (0:Disable)",
                    0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_IDR_PERIOD,
            g_param_spec_uint ("force-idr-period", "Specifies periodicity of IDR frames",
                    "Specifies periodicity of IDR frames (0:Disable)",
                    0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_FORCE_IDR,
            g_param_spec_boolean ("force-idr", "force-idr", "force next frame to be IDR",
                    FALSE, G_PARAM_WRITABLE));


    }
}

static void
omx_h264_push_cb (GstOmxBaseFilter *omx_base, GstBuffer *buf)
{
    static guint cont;
    GstOmxH264Enc *self;
    self = GST_OMX_H264ENC (omx_base);

    /* Currently we use this logic to handle IDR period since the latest
     * EZSDK version doesn't have support for OMX_IndexConfigVideoAVCIntraPeriod
	 */
    if ((self->idr_period > 0) || (self->force_idr))
    {
        if ((cont == self->idr_period) || (self->force_idr))
        {
            OMX_CONFIG_INTRAREFRESHVOPTYPE confIntraRefreshVOP;

            _G_OMX_INIT_PARAM (&confIntraRefreshVOP);
            confIntraRefreshVOP.nPortIndex = omx_base->out_port->port_index;

            OMX_GetConfig (g_omx_core_get_handle (omx_base->gomx),
                              OMX_IndexConfigVideoIntraVOPRefresh,
                              &confIntraRefreshVOP);
            confIntraRefreshVOP.IntraRefreshVOP = TRUE;

            OMX_SetConfig (g_omx_core_get_handle (omx_base->gomx),
                           OMX_IndexConfigVideoIntraVOPRefresh,
                           &confIntraRefreshVOP);

            if (cont == self->idr_period)
                cont = 0;

            if (self->force_idr)
            {
                self->force_idr = FALSE;
                cont++;
            }
        } else {
            cont++;
		}
    }
	GST_BUFFER_CAPS(buf) = gst_caps_ref(GST_PAD_CAPS(omx_base->srcpad));
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVideoEnc *self;
    GOmxCore *gomx;

    self = GST_OMX_BASE_VIDEOENC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    {
        OMX_INDEXTYPE index;

        if (OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.VideoEncode.Config.NALFormat", &index) == OMX_ErrorNone)
        {
            OMX_U32 nal_format;
            GstOmxH264Enc *h264enc;

            h264enc = GST_OMX_H264ENC (omx_base);
            nal_format = h264enc->bytestream ? 0 : 1;
            GST_DEBUG_OBJECT (omx_base, "setting 'OMX.TI.VideoEncode.Config.NALFormat' to %ld", nal_format);

            OMX_SetParameter (gomx->omx_handle, index, &nal_format);
        }
        else
        {
            GST_WARNING_OBJECT (omx_base, "'OMX.TI.VideoEncode.Config.NALFormat' unsupported");
        }
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

        G_OMX_PORT_GET_DEFINITION (omx_base_filter->out_port, &param);
        width = param.format.video.nFrameWidth;
        height = param.format.video.nFrameHeight;
    }

    {
        GstCaps *new_caps;

        new_caps = gst_caps_new_simple ("video/x-h264",
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height,
                                        "framerate", GST_TYPE_FRACTION,
                                        omx_base->framerate_num, omx_base->framerate_denom,
										"stream-format", G_TYPE_STRING, "byte-stream",
										"alignment", G_TYPE_STRING, "au",
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
    GstOmxBaseFilterClass *bclass;
    GstOmxH264Enc *self;

    omx_base_filter = GST_OMX_BASE_FILTER (instance);
    omx_base = GST_OMX_BASE_VIDEOENC (instance);
    self = GST_OMX_H264ENC (instance);
    bclass = GST_OMX_BASE_FILTER_CLASS (g_class);

    omx_base->omx_setup = omx_setup;

    omx_base_filter->push_cb = omx_h264_push_cb;

    omx_base->compression_format = OMX_VIDEO_CodingAVC;

    omx_base_filter->gomx->settings_changed_cb = settings_changed_cb;

    self->idr_period = 0;
    self->force_idr = FALSE;
}
