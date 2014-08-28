/* GStreamer
 *
 * Copyright (C) 2011 RidgeRun
 * Copyright (C) 2009 Texas Instruments, Inc - http://www.ti.com/
 *
 * Description: OMX Camera element for DM81xx
 * David Soto  <david.soto@ridgerun.com>
 *
 * Based on gstomx_camera by Rob Clark <rob@ti.com>
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
 */

#include "gstomx_camera.h"
#include "gstomx.h"

#include <OMX_TI_Common.h>
#include <omx_vfcc.h>
#include <omx_ctrl.h>
#include <gst/video/video.h>

#include <OMX_TI_IVCommon.h>
#include <OMX_TI_Index.h>

#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <OMX_CoreExt.h>
#include <OMX_IndexExt.h>

char* PORT_NAMES(int i)
{
	switch (i)
	{
		case 1: return "VIP1_PORTA";
		case 2: return "VIP1_PORTB";
		case 3: return "VIP2_PORTA";
		case 4: return "VIP2_PORTB";
		break;
	}
	return NULL;
}

char* MODE_NAMES(int i)
{
	switch (i)
	{
		case 1: return "SC_NON_MUX";
		case 2: return "MC_LINE_MUX";
		case 3: return "MC_PEL_MUX";
		case 4: return "SC_DISCRETESYNC";
		case 5: return "MC_LINE_MUX_SPLIT_LINE";
		case 6: return "SC_DISCRETESYNC_ACTVID_VSYNC";
		break;
	}
	return NULL;
}

char* VIF_NAMES(int i)
{
	switch (i)
	{
		case 1: return "08BIT";
		case 2: return "16BIT";
		case 3: return "24BIT";
		break;
	}
	return NULL;
}

char* TYPE_NAMES(int i)
{
	switch (i)
	{
		case 1: return "Progressive";
		case 2: return "Interlaced";
		break;
	}
	return NULL;
}

#define MAX_SHIFTS	30
/**
 * SECTION:element-omx_camera
 *
 * omx_camerasrc can be used to capture video and/or still frames from OMX
 * camera.<p>
 */
static const GstElementDetails element_details =
GST_ELEMENT_DETAILS ("Video OMX Camera Source",
    "Source/Video",
    "Reads frames from an OMX Camera Component",
    "David Soto <david.soto@ridgerun.com>");
/*
 * Properties
 */
enum
{
  ARG_0,
  ARG_INPUT_INTERFACE,
  ARG_CAP_MODE,
  ARG_SCAN_TYPE,
  ARG_SKIP_FRAMES,
  ARG_VIF_MODE,
  ARG_OVERRIDE_COLORSPACE
};

GSTOMX_BOILERPLATE (GstOmxCamera, gst_omx_camera, GstOmxBaseSrc,
    GST_OMX_BASE_SRC_TYPE);

/*
 * Caps:
 */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (GSTOMX_ALL_FORMATS,
            "[ 0, max ]"))
    );

static gboolean
src_setcaps (GstPad * pad, GstCaps * caps)
{
  GstOmxCamera *self = GST_OMX_CAMERA (GST_PAD_PARENT (pad));
  GstOmxBaseSrc *omx_base = GST_OMX_BASE_SRC (self);

  GstVideoFormat format;
  gint width, height, rowstride;
  OMX_ERRORTYPE err;
  GstStructure *structure;

  if (!self) {
    GST_DEBUG_OBJECT (pad, "pad has no parent (yet?)");
    return TRUE;                // ???
  }

  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  if (gst_video_format_parse_caps_strided (caps,
          &format, &width, &height, &rowstride)) {

    /* Output port configuration: */
    OMX_PARAM_PORTDEFINITIONTYPE param;
    gboolean configure_port = FALSE;

    G_OMX_PORT_GET_DEFINITION (self->port, &param);

    if ((param.format.video.nFrameWidth != width) ||
        (param.format.video.nFrameHeight != height) ||
        (param.format.video.nStride != rowstride)) {
      param.format.video.nFrameWidth = width;
      param.format.video.nFrameHeight = height;
      param.format.video.nStride = self->rowstride = width;
      configure_port = TRUE;
    }

    param.nBufferSize = gst_video_format_get_size_strided (format,
        width, height, rowstride);
    if (self->scan_type == OMX_VIDEO_CaptureScanTypeInterlaced)
      height = height / 2;
    /* special hack to work around OMX camera bug:
     */
    if (param.format.video.eColorFormat !=
        g_omx_gstvformat_to_colorformat (format)) {
      if (g_omx_gstvformat_to_colorformat (format) ==
          OMX_COLOR_FormatYUV420PackedSemiPlanar) {
        if (param.format.video.eColorFormat != OMX_COLOR_FormatYUV420SemiPlanar) {
          param.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
          configure_port = TRUE;
        }
      } else {
        param.format.video.eColorFormat =
            g_omx_gstvformat_to_colorformat (format);
        configure_port = TRUE;
      }
    }

	if (self->override_colorspace)
	{
		param.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
		configure_port = TRUE;
		printf("OVERRIDING COLORSPACE (forcing YUY2).....\n");
	}

    /*Setting compression Format */
    param.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    if (configure_port) {

      gboolean port_enabled = FALSE;

      if (self->port->enabled && (omx_base->gomx->omx_state != OMX_StateLoaded)) {
        g_omx_port_disable (self->port);
        port_enabled = TRUE;
      }

      err = G_OMX_PORT_SET_DEFINITION (self->port, &param);
      if (err != OMX_ErrorNone)
        return FALSE;

      if (port_enabled)
        g_omx_port_enable (self->port);
    }

    /* Setting Memory type at output port to Raw Memory */
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    G_OMX_PORT_GET_DEFINITION (self->port, &memTypeCfg);
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    G_OMX_PORT_SET_PARAM (self->port,
        OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    /* capture on EIO card is component input at VIP1 port */
    OMX_PARAM_VFCC_HWPORT_ID sHwPortId;
    _G_OMX_INIT_PARAM (&sHwPortId);
    sHwPortId.eHwPortId = self->input_interface;
printf("INPUT INTERFACE: %s\n", PORT_NAMES(sHwPortId.eHwPortId));
    G_OMX_PORT_SET_PARAM (self->port,
        (OMX_INDEXTYPE) OMX_TI_IndexParamVFCCHwPortID, (OMX_PTR) & sHwPortId);

    OMX_PARAM_VFCC_HWPORT_PROPERTIES sHwPortParam;
    _G_OMX_INIT_PARAM (&sHwPortParam);
    sHwPortParam.eCaptMode = self->cap_mode;
printf("CAP MODE: %s\n", MODE_NAMES(sHwPortParam.eCaptMode));
    sHwPortParam.eVifMode = self->vif_mode;
printf("VIF MODE: %s\n", VIF_NAMES(sHwPortParam.eVifMode));
    sHwPortParam.nMaxHeight = height;
    sHwPortParam.nMaxWidth = width;
    sHwPortParam.nMaxChnlsPerHwPort = 1;
    sHwPortParam.eScanType = self->scan_type;
printf("INTERLACE MODE: %s\n", TYPE_NAMES(sHwPortParam.eScanType));

    structure = gst_caps_get_structure (caps, 0);

    if (!strcmp (gst_structure_get_name (structure), "video/x-raw-yuv")) {
      self->input_format = OMX_COLOR_FormatYCbYCr;
    } else if (!strcmp (gst_structure_get_name (structure), "video/x-raw-rgb")) {
      self->input_format = OMX_COLOR_Format24bitRGB888;
    } else
      return FALSE;

    sHwPortParam.eInColorFormat = self->input_format;
    G_OMX_PORT_SET_PARAM (self->port,
        (OMX_INDEXTYPE) OMX_TI_IndexParamVFCCHwPortProperties,
        (OMX_PTR) & sHwPortParam);

    if (!gst_pad_set_caps (GST_BASE_SRC_PAD (self), caps))
      return FALSE;
  }
  return TRUE;
}

static void
setup_ports (GstOmxBaseSrc * base_src)
{
  GstOmxCamera *self = GST_OMX_CAMERA (base_src);

  /*Configuring port to allocated buffers instead of use shared buffers */
  self->port->omx_allocate = TRUE;
  self->port->share_buffer = FALSE;
}

static GstClockTime
get_timestamp (GstOmxCamera * self)
{
  GstClock *clock;
  GstClockTime timestamp;

  /* timestamps, LOCK to get clock and base time. */
  GST_OBJECT_LOCK (self);
  if ((clock = GST_ELEMENT_CLOCK (self))) {
    /* we have a clock, get base time and ref clock */
    timestamp = GST_ELEMENT (self)->base_time;
    gst_object_ref (clock);
  } else {
    /* no clock, can't set timestamps */
    timestamp = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (self);

  if (clock) {
    /* the time now is the time of the clock minus the base time */
    /* Hack: Need to subtract the extra lag that is causing problems to AV sync */
    timestamp = gst_clock_get_time (clock) - timestamp - (65 * GST_MSECOND);
    gst_object_unref (clock);

    /* if we have a framerate adjust timestamp for frame latency */
#if 0
    if (self->fps_n > 0 && self->fps_d > 0) {
      GstClockTime latency;

      latency =
          gst_util_uint64_scale_int (GST_SECOND, self->fps_d, self->fps_n);

      if (timestamp > latency)
        timestamp -= latency;
      else
        timestamp = 0;
    }
#endif
  }
  return timestamp;
}

static void
start_ports (GstOmxCamera * self)
{
  g_omx_port_enable (self->port);
}

/*
 * GstBaseSrc Methods:
 */

static GstFlowReturn
create (GstBaseSrc * gst_base,
    guint64 offset, guint length, GstBuffer ** ret_buf)
{
  GstOmxCamera *self = GST_OMX_CAMERA (gst_base);
  GstOmxBaseSrc *omx_base = GST_OMX_BASE_SRC (self);
  GstFlowReturn ret = GST_FLOW_NOT_NEGOTIATED;
  guint n_offset = 0;

  if (omx_base->gomx->omx_state == OMX_StateLoaded) {
    gst_omx_base_src_setup_ports (omx_base);
    g_omx_core_prepare (omx_base->gomx);
  }

  if (!self->alreadystarted) {
    self->alreadystarted = 1;
    start_ports (self);
  }

  ret = gst_omx_base_src_create_from_port (omx_base, self->port, ret_buf);

  n_offset = self->port->n_offset;

  if (ret != GST_FLOW_OK)
    goto fail;

  GST_BUFFER_TIMESTAMP (*ret_buf) = get_timestamp (self);

  return GST_FLOW_OK;

fail:
  if (*ret_buf)
    gst_buffer_unref (*ret_buf);
  return ret;
}

/*
 * GObject Methods:
 */
static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxCamera *self = GST_OMX_CAMERA (obj);
  GstOmxBaseSrc *omx_base = GST_OMX_BASE_SRC (self);
  gchar *str_value;
  g_free (str_value);

  switch (prop_id) {
    case ARG_INPUT_INTERFACE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "VIP1_PORTA")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP1_PORTA;
      } else if (!strcmp (str_value, "VIP2_PORTA")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP2_PORTA;
      } else if (!strcmp (str_value, "VIP1_PORTB")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP1_PORTB;
      } else if (!strcmp (str_value, "VIP2_PORTB")) {
        self->input_interface = OMX_VIDEO_CaptureHWPortVIP2_PORTB;
      } else {
        GST_WARNING_OBJECT (omx_base, "%s unsupported", str_value);
        g_return_if_fail (0);
      }
      break;
    }
    case ARG_CAP_MODE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "MC_LINE_MUX"))
        self->cap_mode = OMX_VIDEO_CaptureModeMC_LINE_MUX;
      else if (!strcmp (str_value, "SC_NON_MUX"))
        self->cap_mode = OMX_VIDEO_CaptureModeSC_NON_MUX;
	  else if (!strcmp (str_value, "MC_PEL_MUX"))
		  self->cap_mode = OMX_VIDEO_CaptureModeMC_PEL_MUX;
	  else if (!strcmp (str_value, "SC_DISCRETESYNC"))
		  self->cap_mode = OMX_VIDEO_CaptureModeSC_DISCRETESYNC;
	  else if (!strcmp (str_value, "MC_LINE_MUX_SPLIT_LINE"))
		  self->cap_mode = OMX_VIDEO_CaptureModeMC_LINE_MUX_SPLIT_LINE;
	  else if (!strcmp (str_value, "SC_DISCRETESYNC_ACTVID_VSYNC"))
		  self->cap_mode = OMX_VIDEO_CaptureModeSC_DISCRETESYNC_ACTVID_VSYNC;
      else
	  {
        GST_WARNING_OBJECT (omx_base, "%s unsupported", str_value);
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
        GST_WARNING_OBJECT (omx_base, "%s unsupported", str_value);
        g_return_if_fail (0);
      }
      break;
    }
    case ARG_SKIP_FRAMES:
    {
      OMX_CONFIG_VFCC_FRAMESKIP_INFO sCapSkipFrames;
      _G_OMX_INIT_PARAM (&sCapSkipFrames);
      guint32 shifts = 0, skip = 0, i = 0, count = 0;
      shifts = g_value_get_uint (value);
      if (shifts) {
        while (count < MAX_SHIFTS) {

          if ((count + shifts) > MAX_SHIFTS) {
            shifts = MAX_SHIFTS - count;
          }
          for (i = 0; i < shifts; i++) {
            skip = skip << 1;
            skip = skip | 1;
            count++;
          }
          if (count < MAX_SHIFTS) {
            skip = skip << 1;
            count++;
          }
        }
      }
	  /*OMX_TI_IndexConfigVFCCFrameSkip is for dropping frames in capture,
	    it is a binary 30bit value where 1 means drop a frame and 0
		process the frame
	    */
      sCapSkipFrames.frameSkipMask = skip;
      G_OMX_PORT_SET_CONFIG (self->port,
          OMX_TI_IndexConfigVFCCFrameSkip, &sCapSkipFrames);
      break;
    }

	case ARG_VIF_MODE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "24BIT"))
        self->vif_mode = OMX_VIDEO_CaptureVifMode_24BIT;
      else if (!strcmp (str_value, "16BIT"))
        self->vif_mode = OMX_VIDEO_CaptureVifMode_16BIT;
      else
        self->vif_mode = OMX_VIDEO_CaptureVifMode_08BIT;
      break;
    }

	case ARG_OVERRIDE_COLORSPACE:
    {
      str_value = g_value_dup_string (value);
      if (!strcmp (str_value, "true"))
        self->override_colorspace = TRUE;
      else
        self->override_colorspace = FALSE;
      break;
    }

    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxCamera *self = GST_OMX_CAMERA (obj);

  switch (prop_id) {
    case ARG_INPUT_INTERFACE:
    {
      if (self->input_interface == OMX_VIDEO_CaptureHWPortVIP2_PORTA)
        g_value_set_string (value, "VIP2_PORTA");
      else if (self->input_interface == OMX_VIDEO_CaptureHWPortVIP2_PORTB)
        g_value_set_string (value, "VIP2_PORTB");
      else if (self->input_interface == OMX_VIDEO_CaptureHWPortVIP1_PORTB)
        g_value_set_string (value, "VIP1_PORTB");
      else
        g_value_set_string (value, "VIP1_PORTA");
      break;
    }
    case ARG_CAP_MODE:
    {
      if (self->cap_mode == OMX_VIDEO_CaptureModeMC_LINE_MUX)
		  g_value_set_string (value, "MC_LINE_MUX");
	  else if (self->cap_mode == OMX_VIDEO_CaptureModeMC_PEL_MUX)
		  g_value_set_string (value, "MC_PEL_MUX");
	  else if (self->cap_mode == OMX_VIDEO_CaptureModeSC_DISCRETESYNC)
		  g_value_set_string (value, "SC_DISCRETESYNC");
	  else if (self->cap_mode == OMX_VIDEO_CaptureModeMC_LINE_MUX_SPLIT_LINE)
		  g_value_set_string (value, "MC_LINE_MUX_SPLIT_LINE");
	  else if (self->cap_mode == OMX_VIDEO_CaptureModeSC_DISCRETESYNC_ACTVID_VSYNC)
		  g_value_set_string (value, "SC_DISCRETESYNC_ACTVID_VSYNC");
      else
          g_value_set_string (value, "SC_NON_MUX");
      break;
    }
    case ARG_SCAN_TYPE:
    {
      if (self->scan_type == OMX_VIDEO_CaptureScanTypeProgressive)
        g_value_set_string (value, "progressive");
      else
        g_value_set_string (value, "interlaced");
      break;
    }
    case ARG_SKIP_FRAMES:
    {
      OMX_CONFIG_VFCC_FRAMESKIP_INFO sCapSkipFrames;

      G_OMX_PORT_GET_CONFIG (self->port,
          OMX_TI_IndexConfigVFCCFrameSkip, &sCapSkipFrames);

      g_value_set_uint (value, sCapSkipFrames.frameSkipMask);
      break;
    }

	case ARG_VIF_MODE:
    {
      if (self->vif_mode == OMX_VIDEO_CaptureVifMode_24BIT)
        g_value_set_string (value, "24BIT");
      else if (self->vif_mode == OMX_VIDEO_CaptureVifMode_16BIT)
        g_value_set_string (value, "16BIT");
	  else
		g_value_set_string (value, "08BIT");
      break;
    }

	case ARG_OVERRIDE_COLORSPACE:
    {
      if (self->override_colorspace == TRUE)
        g_value_set_string (value, "true");
      else
		g_value_set_string (value, "false");
      break;
    }

    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
  }
}

/*
 * Initialization:
 */

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &element_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
}

static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  GstElementClass *gst_element_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  gst_element_class = GST_ELEMENT_CLASS (g_class);

  GstBaseSrcClass *gst_base_src_class;
  GstOmxBaseSrcClass *omx_base_class;

  gst_base_src_class = GST_BASE_SRC_CLASS (g_class);
  omx_base_class = GST_OMX_BASE_SRC_CLASS (g_class);

  omx_base_class->out_port_index = OMX_CAMERA_PORT_VIDEO_OUT_VIDEO;

  /* GstBaseSrc methods: */
  gst_base_src_class->create = GST_DEBUG_FUNCPTR (create);

  /* GObject methods: */
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

  /* install properties: */
  g_object_class_install_property (gobject_class, ARG_INPUT_INTERFACE,
      g_param_spec_string ("input-interface", "Video input interface",
          "The video input interface from where capture image/video is obtained (see below)"
          "\n\t\t\t VIP1_PORTA "
          "\n\t\t\t VIP1_PORTB "
          "\n\t\t\t VIP2_PORTA "
          "\n\t\t\t VIP2_PORTB ", "VIP1_PORTA", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_CAP_MODE,
      g_param_spec_string ("capture-mode", "Multiplex/Sync mode",
          "Video capture mode (Multiplexed/Sync) (see below)"
          "\n\t\t\t SC_NON_MUX "
		  "\n\t\t\t MC_LINE_MUX "
		  "\n\t\t\t MC_PEL_MUX "
		  "\n\t\t\t SC_DISCRETESYNC "
		  "\n\t\t\t MC_LINE_MUX_SPLIT_LINE "
		  "\n\t\t\t SC_DISCRETESYNC_ACTVID_VSYNC ", "SC_NON_MUX", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_SCAN_TYPE,
      g_param_spec_string ("scan-type", "Video scan mode",
          "Video scan mode (see below)"
          "\n\t\t\t progressive "
          "\n\t\t\t interlaced ", "progressive", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_SKIP_FRAMES,
      g_param_spec_uint ("skip-frames", "skip frames",
          "Skip this amount of frames after a vaild frame",
          0, 30, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_VIF_MODE,
      g_param_spec_string ("vif-mode", "Bit width of capture",
          "Video capture size (8, 16, 24 bits) (see below)"
          "\n\t\t\t 08BIT "
		  "\n\t\t\t 16BIT "
		  "\n\t\t\t 24BIT ", "08BIT", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_OVERRIDE_COLORSPACE,
      g_param_spec_string ("override-colorspace", "Force capture of YUY2 video",
          "Set to true to disable colorspace conversion in the camera"
          "\n\t\t\t true "
		  "\n\t\t\t false ", "false", G_PARAM_READWRITE));

}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxCamera *self = GST_OMX_CAMERA (instance);
  GstOmxBaseSrc *omx_base = GST_OMX_BASE_SRC (self);
  GstBaseSrc *basesrc = GST_BASE_SRC (self);

  self->alreadystarted = 0;

  omx_base->setup_ports = setup_ports;

  omx_base->gomx->use_timestamps = TRUE;

  /*Since OMXBaseSrc already created a port, this function is going
   *to return that port (omx_base->out_port)*/
  self->port = g_omx_core_get_port (omx_base->gomx, "out",
      OMX_CAMERA_PORT_VIDEO_OUT_VIDEO);

  gst_base_src_set_live (basesrc, TRUE);

  /* setup src pad (already created by basesrc): */
  gst_pad_set_setcaps_function (GST_BASE_SRC_PAD (basesrc),
      GST_DEBUG_FUNCPTR (src_setcaps));

  /*Initialize properties */
  self->input_interface = OMX_VIDEO_CaptureHWPortVIP1_PORTA;
  self->cap_mode = OMX_VIDEO_CaptureModeSC_NON_MUX;
  self->scan_type = OMX_VIDEO_CaptureScanTypeProgressive;
  self->vif_mode = OMX_VIDEO_CaptureVifMode_08BIT;
  self->override_colorspace = FALSE;

  /* disable all ports to begin with: */
  g_omx_port_disable (self->port);

}
