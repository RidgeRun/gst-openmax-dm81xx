/*
 * Copyright (C) 2011 RidgeRun.
 *
 * Author: David Soto <david.soto@ridgerun.com>
 *
 * Based on gstomx_base_ctrl.c made by:
 *       Brijesh Singh <bksingh@ti.com>
 *
 * Copyright (C) 2010-2011 Texas Instrument Inc.
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

#include "gstomx_tvp.h"
#include "gstomx.h"
#include <OMX_TI_Common.h>
#include <omx_vfcc.h>
enum
{
  ARG_0,
  ARG_COMPONENT_ROLE,
  ARG_COMPONENT_NAME,
  ARG_LIBRARY_NAME,
  ARG_INPUT_INTERFACE,
  ARG_CAP_MODE,
  ARG_SCAN_TYPE
};

GSTOMX_BOILERPLATE (GstOmxBaseTvp, gst_omx_tvp, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM);

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

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  {
    GstElementDetails details;

    details.longname = "External video decoder control";
    details.klass = "Filter";
    details.description = "OpenMAX video decoder control";
    details.author = "David Soto";

    gst_element_class_set_details (element_class, &details);
  }
}

static gboolean
tvp_setcaps (GstBaseTransform * trans, GstCaps * incaps, GstCaps * outcaps)
{
  GstOmxBaseTvp *self;

  self = GST_OMX_TVP (trans);
  GstStructure *structure;

  if (gst_video_format_parse_caps_strided (incaps,
          &self->format, &self->width, &self->height, &self->rowstride)) {

    structure = gst_caps_get_structure (incaps, 0);
    if (!strcmp (gst_structure_get_name (structure), "video/x-raw-yuv")) {
      self->input_format = OMX_COLOR_FormatYCbYCr;
    } else if (!strcmp (gst_structure_get_name (structure), "video/x-raw-rgb")) {
      self->input_format = OMX_COLOR_Format24bitRGB888;
    } else
      return FALSE;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_omx_configure_tvp (GstOmxBaseTvp * self)
{
  OMX_ERRORTYPE err;
  GOmxCore *gomx;

  gomx = (GOmxCore *) self->gomx;

  /* Set parameters for TVP controller */
  OMX_PARAM_VFCC_HWPORT_ID sHwPortId;
  _G_OMX_INIT_PARAM (&sHwPortId);

  /* capture on EIO card */
  sHwPortId.eHwPortId = self->input_interface;
  err = OMX_SetParameter (g_omx_core_get_handle (gomx),
      (OMX_INDEXTYPE) OMX_TI_IndexParamVFCCHwPortID, (OMX_PTR) & sHwPortId);

  if (err != OMX_ErrorNone)
    return FALSE;

  OMX_PARAM_VFCC_HWPORT_PROPERTIES sHwPortParam;
  _G_OMX_INIT_PARAM (&sHwPortParam);
  sHwPortParam.eCaptMode = self->cap_mode;
  sHwPortParam.eVifMode = OMX_VIDEO_CaptureVifMode_16BIT;
  sHwPortParam.eInColorFormat = self->input_format;
  sHwPortParam.eScanType = self->scan_type;
  sHwPortParam.nMaxHeight = self->height;
  sHwPortParam.nMaxWidth = self->width;
  sHwPortParam.nMaxChnlsPerHwPort = 1;

  err = OMX_SetParameter (g_omx_core_get_handle (gomx),
      (OMX_INDEXTYPE) OMX_TI_IndexParamVFCCHwPortProperties,
      (OMX_PTR) & sHwPortParam);

  if (err != OMX_ErrorNone)
    return FALSE;

  /* set the mode based on capture/display device */
  OMX_PARAM_CTRL_VIDDECODER_INFO sVidDecParam;
  _G_OMX_INIT_PARAM (&sVidDecParam);
  if (self->scan_type == OMX_VIDEO_CaptureScanTypeProgressive) {
    if (self->height == 720)
      sVidDecParam.videoStandard = OMX_VIDEO_DECODER_STD_720P_60;
    else
      sVidDecParam.videoStandard = OMX_VIDEO_DECODER_STD_1080P_60;
  } else
    sVidDecParam.videoStandard = OMX_VIDEO_DECODER_STD_1080I_60;


  /* setting TVP7002 component input */
  sVidDecParam.videoDecoderId = OMX_VID_DEC_TVP7002_DRV;
  sVidDecParam.videoSystemId = OMX_VIDEO_DECODER_VIDEO_SYSTEM_AUTO_DETECT;
  err = OMX_SetParameter (g_omx_core_get_handle (gomx),
      (OMX_INDEXTYPE) OMX_TI_IndexParamCTRLVidDecInfo,
      (OMX_PTR) & sVidDecParam);


  if (err != OMX_ErrorNone)
    return FALSE;

  g_omx_core_change_state (gomx, OMX_StateIdle);
  g_omx_core_change_state (gomx, OMX_StateExecuting);
  self->mode_configured = TRUE;

  return TRUE;
}

static gboolean
tvp_start (GstBaseTransform * trans)
{
  GstOmxBaseTvp *self;

  self = GST_OMX_TVP (trans);

  g_omx_core_init (self->gomx);
  gst_omx_configure_tvp (self);
  return TRUE;
}

static gboolean
tvp_stop (GstBaseTransform * trans)
{
  GstOmxBaseTvp *self;

  self = GST_OMX_TVP (trans);

  g_omx_core_stop (self->gomx);
  g_omx_core_unload (self->gomx);

  g_omx_core_free (self->gomx);
  return TRUE;
}

static GstFlowReturn
tvp_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstOmxBaseTvp *self;
  GstFlowReturn ret = GST_FLOW_OK;

  self = GST_OMX_TVP (trans);

  /* if mode is already configure then return */
  if (self->mode_configured)
    return ret;

  if (!gst_omx_configure_tvp (self))
    ret = GST_FLOW_ERROR;

  return ret;
}

tvp_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * in_buf, gint out_size, GstCaps * out_caps, GstBuffer ** out_buf)
{
  *out_buf = gst_buffer_ref (in_buf);
  return GST_FLOW_OK;
}

static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxBaseTvp *self;
  self = GST_OMX_TVP (obj);
  gchar *str_value;
  g_free (str_value);

  switch (prop_id) {
    case ARG_COMPONENT_ROLE:
      g_free (self->omx_role);
      self->omx_role = g_value_dup_string (value);
      break;
    case ARG_COMPONENT_NAME:
      g_free (self->omx_component);
      self->omx_component = g_value_dup_string (value);
      break;
    case ARG_LIBRARY_NAME:
      g_free (self->omx_library);
      self->omx_library = g_value_dup_string (value);
      break;
    case ARG_INPUT_INTERFACE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "VIP1_PORTA")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP1_PORTA;
      } else if (!strcmp (str_value, "VIP2_PORTA")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP2_PORTA;
      } else {
        GST_WARNING_OBJECT (self, "%s unsupported", str_value);
        g_return_if_fail (0);
      }
      break;
    }
    case ARG_CAP_MODE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "MC_LINE_MUX")) {
        self->cap_mode = OMX_VIDEO_CaptureModeMC_LINE_MUX;
      } else if (!strcmp (str_value, "SC_NON_MUX")) {
        self->cap_mode = OMX_VIDEO_CaptureModeSC_NON_MUX;
      } else {
        GST_WARNING_OBJECT (self, "%s unsupported", str_value);
        g_return_if_fail (0);
      }
      break;
    }
    case ARG_SCAN_TYPE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "progressive")) {
        self->scan_type = OMX_VIDEO_CaptureScanTypeProgressive;
      } else if (!strcmp (str_value, "interlaced")) {
        self->scan_type = OMX_VIDEO_CaptureScanTypeInterlaced;
      } else {
        GST_WARNING_OBJECT (self, "%s unsupported", str_value);
        g_return_if_fail (0);
      }
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxBaseTvp *self;

  self = GST_OMX_TVP (obj);

  switch (prop_id) {
    case ARG_COMPONENT_ROLE:
      g_value_set_string (value, self->omx_role);
      break;
    case ARG_COMPONENT_NAME:
      g_value_set_string (value, self->omx_component);
      break;
    case ARG_LIBRARY_NAME:
      g_value_set_string (value, self->omx_library);
      break;
    case ARG_INPUT_INTERFACE:
      if (self->input_interface == OMX_VIDEO_CaptureHWPortVIP2_PORTA)
        g_value_set_string (value, "VIP2_PORTA");
      else
        g_value_set_string (value, "VIP1_PORTA");
      break;
    case ARG_CAP_MODE:
      if (self->cap_mode == OMX_VIDEO_CaptureModeMC_LINE_MUX)
        g_value_set_string (value, "MC_LINE_MUX");
      else
        g_value_set_string (value, "SC_NON_MUX");
      break;
    case ARG_SCAN_TYPE:
      if (self->scan_type == OMX_VIDEO_CaptureScanTypeProgressive)
        g_value_set_string (value, "progressive");
      else
        g_value_set_string (value, "interlaced");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
finalize (GObject * obj)
{
  GstOmxBaseTvp *self;

  self = GST_OMX_TVP (obj);

  g_free (self->omx_role);
  g_free (self->omx_component);
  g_free (self->omx_library);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  GstOmxBaseTvp *gst_base_ctrl_class;
  GstOmxBaseTvpClass *omx_base_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  gst_base_ctrl_class = GST_OMX_TVP (g_class);
  omx_base_class = GST_OMX_TVP_CLASS (g_class);

  gobject_class->finalize = finalize;

  /* Properties stuff */
  {
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    g_object_class_install_property (gobject_class, ARG_COMPONENT_ROLE,
        g_param_spec_string ("component-role", "Component role",
            "Role of the OpenMAX IL component", NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_COMPONENT_NAME,
        g_param_spec_string ("component-name", "Component name",
            "Name of the OpenMAX IL component to use",
            NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_LIBRARY_NAME,
        g_param_spec_string ("library-name", "Library name",
            "Name of the OpenMAX IL implementation library to use",
            NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_INPUT_INTERFACE,
        g_param_spec_string ("input-interface", "Video input interface",
            "The video input interface from where capture image/video is obtained (see below)"
            "\n\t\t\t VIP1_PORTA "
            "\n\t\t\t VIP2_PORTA ", "VIP1_PORTA", G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_CAP_MODE,
        g_param_spec_string ("capture-mode", "Multiplex/Sync mode",
            "Video capture mode (Multiplexed/Sync) (see below)"
            "\n\t\t\t MC_LINE_MUX "
            "\n\t\t\t SC_NON_MUX ", "SC_NON_MUX", G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_SCAN_TYPE,
        g_param_spec_string ("scan-type", "Video scan mode",
            "Video scan mode (see below)"
            "\n\t\t\t progressive "
            "\n\t\t\t interlaced ", "progressive", G_PARAM_READWRITE));

  }
}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{

  GstOmxBaseTvp *self;
  GstOmxBaseTvpClass *klass;
  GstBaseTransformClass *trans_class;

  self = GST_OMX_TVP (instance);
  klass = GST_OMX_TVP_CLASS (g_class);
  trans_class = (GstBaseTransformClass *) klass;

  self->gomx = g_omx_core_new (self, g_class);
  self->in_port = g_omx_core_get_port (self->gomx, "in", 0);

  trans_class->passthrough_on_same_caps = TRUE;
  trans_class->set_caps = GST_DEBUG_FUNCPTR (tvp_setcaps);
  trans_class->transform_ip = GST_DEBUG_FUNCPTR (tvp_transform_ip);
  trans_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (tvp_prepare_output_buffer);
  trans_class->start = GST_DEBUG_FUNCPTR (tvp_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (tvp_stop);

  self->input_interface = OMX_VIDEO_CaptureHWPortVIP1_PORTA;
  self->cap_mode = OMX_VIDEO_CaptureModeSC_NON_MUX;
  self->scan_type = OMX_VIDEO_CaptureScanTypeProgressive;
  self->mode_configured = FALSE;

}
