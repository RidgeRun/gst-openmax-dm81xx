/*
 * Copyright (C) 2008-2009 Nokia Corporation.
 *
 * Author: David Soto <david.soto@ridgerun.com>
 * Copyright (C) 2013 RidgeRun
 *
 * Based on the plugin version created by: 
 *            Felipe Contreras <felipe.contreras@nokia.com>
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
#include "gstomx.h"
#include <OMX_TI_Index.h>
#include <OMX_TI_Video.h>

#include <string.h>             /* for memset */

GSTOMX_BOILERPLATE (GstOmxMjpegEnc, gst_omx_mjpegenc, GstOmxBaseVideoEnc,
    GST_OMX_BASE_VIDEOENC_TYPE);

enum
{
  ARG_0,
  ARG_QUALITY,
};

static GstCaps *
generate_src_template (void)
{
  GstCaps *caps;

  caps = gst_caps_new_simple ("image/jpeg",
      "width", GST_TYPE_INT_RANGE, 16, 4096,
      "height", GST_TYPE_INT_RANGE, 16, 4096,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  return caps;
}

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);

  {
    GstElementDetails details;

    details.longname = "OpenMAX IL MJPEG video encoder";
    details.klass = "Codec/Encoder/Video";
    details.description = "Encodes video in MJPEG format with OpenMAX IL";
    details.author = "David Soto";

    gst_element_class_set_details (element_class, &details);
  }

  {
    GstPadTemplate *template;

    template = gst_pad_template_new ("src", GST_PAD_SRC,
        GST_PAD_ALWAYS, generate_src_template ());

    gst_element_class_add_pad_template (element_class, template);
  }
}

static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxBaseFilter *omx_base;
  GOmxCore *gomx;
  GstOmxMjpegEnc *self;

  omx_base = GST_OMX_BASE_FILTER (obj);
  self = GST_OMX_MJPEGENC (obj);
  gomx = (GOmxCore *) omx_base->gomx;

  switch (prop_id) {
    case ARG_QUALITY:
#if 0
    {
      OMX_IMAGE_PARAM_QFACTORTYPE tQualityFactor;
      OMX_ERRORTYPE error_val = OMX_ErrorNone;
      
      _G_OMX_INIT_PARAM(&tQualityFactor);
      tQualityFactor.nPortIndex = omx_base->out_port->port_index;
      
      error_val = OMX_GetParameter(gomx->omx_handle, OMX_IndexParamQFactor, &tQualityFactor);
      
      GST_INFO_OBJECT (self, "At set property QFactor %d",
                       (gint)tQualityFactor.nQFactor);
      
      g_assert (error_val == OMX_ErrorNone);
            
      tQualityFactor.nQFactor = g_value_get_uint (value);

      error_val = OMX_SetParameter(gomx->omx_handle, OMX_IndexParamQFactor,  &tQualityFactor);
      
      OMX_GetParameter(gomx->omx_handle, OMX_IndexParamQFactor, &tQualityFactor);
        
      GST_INFO_OBJECT (self, "Exit setup QFactor %d",
                      (gint)tQualityFactor.nQFactor);
      g_assert (error_val == OMX_ErrorNone);

      break;
    }
#else
    self->quality = g_value_get_uint (value);
    break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxMjpegEnc *self;
  GOmxCore *gomx;
  GstOmxBaseFilter *omx_base;

  omx_base = GST_OMX_BASE_FILTER (obj);
  self = GST_OMX_MJPEGENC (obj);
  gomx = (GOmxCore *) omx_base->gomx;

  switch (prop_id) {
    case ARG_QUALITY:
#if 0
    {
      OMX_IMAGE_PARAM_QFACTORTYPE tQualityFactor;
      OMX_ERRORTYPE error_val = OMX_ErrorNone;
      
      _G_OMX_INIT_PARAM(&tQualityFactor);
      tQualityFactor.nPortIndex = omx_base->out_port->port_index;

      error_val = OMX_GetParameter(gomx->omx_handle, OMX_IndexParamQFactor, &tQualityFactor);
      
      GST_INFO_OBJECT (self, "At get property QFactor %d",
                       (gint)tQualityFactor.nQFactor);
      
      g_assert (error_val == OMX_ErrorNone);
            
      g_value_set_uint(value, (gint)tQualityFactor.nQFactor);

      break;
    }
#else
    g_value_set_uint(value, self->quality);
    break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (g_class);

  /* Properties stuff */
  {
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;


    g_object_class_install_property (gobject_class, ARG_QUALITY,
        g_param_spec_uint ("quality", "MJPEG/JPEG quality",
            "MJPEG/JPEG quality (integer 0:min 100:max)",
            0, 100, 90, G_PARAM_READWRITE));
  }
}

static void
omx_mjpeg_push_cb (GstOmxBaseFilter * omx_base, GstBuffer * buf)
{
  GstOmxMjpegEnc *self;
  self = GST_OMX_MJPEGENC (omx_base);

  GST_BUFFER_CAPS (buf) = gst_caps_ref (GST_PAD_CAPS (omx_base->srcpad));
}

static void
omx_setup (GstOmxBaseFilter * omx_base)
{
  GstOmxBaseVideoEnc *self;
  GOmxCore *gomx;
  GstOmxMjpegEnc *mjpegenc;

  mjpegenc = GST_OMX_MJPEGENC (omx_base);
  self = GST_OMX_BASE_VIDEOENC (omx_base);
  gomx = (GOmxCore *) omx_base->gomx;

  GST_INFO_OBJECT (omx_base, "begin");

   {
        OMX_IMAGE_PARAM_QFACTORTYPE tQualityFactor;

        _G_OMX_INIT_PARAM(&tQualityFactor);
        tQualityFactor.nPortIndex = omx_base->out_port->port_index;

        OMX_GetParameter(gomx->omx_handle, OMX_IndexParamQFactor, &tQualityFactor);
        
        GST_INFO_OBJECT (self, "At setup QFactor %d",
                       (gint)tQualityFactor.nQFactor);

        tQualityFactor.nQFactor = mjpegenc->quality;

        OMX_SetParameter(gomx->omx_handle, OMX_IndexParamQFactor,  &tQualityFactor);
        
        OMX_GetParameter(gomx->omx_handle, OMX_IndexParamQFactor, &tQualityFactor);
        
        GST_INFO_OBJECT (self, "Exit setup QFactor %d",
                       (gint)tQualityFactor.nQFactor);
    }

  GST_INFO_OBJECT (omx_base, "end");

}

static void
settings_changed_cb (GOmxCore * core)
{
  GstOmxBaseVideoEnc *omx_base;
  GstOmxBaseFilter *omx_base_filter;
  guint width;
  guint height;

  omx_base_filter = core->object;
  omx_base = GST_OMX_BASE_VIDEOENC (omx_base_filter);

  GST_INFO_OBJECT (omx_base, "settings changed");

  {
    OMX_PARAM_PORTDEFINITIONTYPE param;

    G_OMX_PORT_GET_DEFINITION (omx_base_filter->out_port, &param);
    width = param.format.video.nFrameWidth;
    height = param.format.video.nFrameHeight;
  }

  {
    GstCaps *new_caps;

    new_caps = gst_caps_new_simple ("image/jpeg",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION,
        omx_base->framerate_num, omx_base->framerate_denom, NULL);

    GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
    gst_pad_set_caps (omx_base_filter->srcpad, new_caps);
  }
}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseFilter *omx_base_filter;
  GstOmxBaseVideoEnc *omx_base;
  GstOmxBaseFilterClass *bclass;
  GstOmxMjpegEnc *self;

  omx_base_filter = GST_OMX_BASE_FILTER (instance);
  omx_base = GST_OMX_BASE_VIDEOENC (instance);
  self = GST_OMX_MJPEGENC (instance);
  bclass = GST_OMX_BASE_FILTER_CLASS (g_class);

  omx_base->omx_setup = omx_setup;

  omx_base_filter->push_cb = omx_mjpeg_push_cb;

  omx_base->compression_format = OMX_VIDEO_CodingMJPEG;

  omx_base_filter->gomx->settings_changed_cb = settings_changed_cb;

  self->quality = 90;
  
}
