/*
 * Copyright (C) 2011-2012 Texas Instruments Inc.
 *
 * Author: Brijesh Singh <bksingh@ti.com>
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

#include "gstomx_base_ctrl.h"
#include "gstomx.h"

enum
{
    ARG_0,
    ARG_COMPONENT_ROLE,
    ARG_COMPONENT_NAME,
    ARG_LIBRARY_NAME,
    ARG_DISPLAY_MODE,
    ARG_DISPLAY_DEVICE
};

GSTOMX_BOILERPLATE (GstOmxBaseCtrl, gst_omx_base_ctrl, GstBaseTransform, GST_TYPE_BASE_TRANSFORM);

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

        details.longname = "OpenMAX IL Client to control display/capture mode";
        details.klass = "Filter";
        details.description = "OpenMAX IL client to control display/capture modes";
        details.author = "Brijesh Singh";

        gst_element_class_set_details (element_class, &details);
    }

}

static int
gst_omx_display_string_to_mode (char *str)
{
    if (!strcmp (str, "OMX_DC_MODE_1080P_30"))
        return OMX_DC_MODE_1080P_30;

    if (!strcmp (str, "OMX_DC_MODE_1080I_60"))
        return OMX_DC_MODE_1080I_60;

    if (!strcmp (str, "OMX_DC_MODE_720P_60"))
        return OMX_DC_MODE_720P_60;

    if (!strcmp (str, "OMX_DC_MODE_1080P_60"))
        return OMX_DC_MODE_1080P_60;
   
    if (!strcmp (str, "OMX_DC_MODE_PAL"))
        return OMX_DC_MODE_PAL;

    if (!strcmp (str, "OMX_DC_MODE_NTSC"))
        return OMX_DC_MODE_NTSC;

    return -1;
}

#define LCD_WIDTH         (800)
#define LCD_HEIGHT        (480)
#define LCD_PIXEL_CLOCK   (33500)
#define LCD_H_FRONT_PORCH (164)
#define LCD_H_BACK_PORCH  (89)
#define LCD_H_SYNC_LENGTH (10)
#define LCD_V_FRONT_PORCH (10)
#define LCD_V_BACK_PORCH  (23)
#define LCD_V_SYNC_LENGTH (10)

static gboolean 
gst_omx_ctrl_set_display_mode (GstOmxBaseCtrl *self)
{
    OMX_PARAM_VFDC_DRIVERINSTID driverId;
    OMX_ERRORTYPE err;
    GOmxCore *gomx;
	OMX_PARAM_DC_CUSTOM_MODE_INFO customModeInfo;
	guint isLCD;

    gomx = (GOmxCore*) self->gomx;
    
  //  printf ("******** installing kernel modules **************\n");
  //  system ("insmod vpss.ko sbufaddr=0xbfb00000");
  //  system ("insmod ti81xxhdmi.ko");
  //  system ("echo 1080P-60 > /sys/devices/platform/vpss/display0/mode");

    GST_LOG_OBJECT (self, "setting display mode to: %s", self->display_mode);

    _G_OMX_INIT_PARAM (&driverId);

	if(!strcmp(self->display_device,"LCD")) {
      driverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD1;
      driverId.eDispVencMode = OMX_DC_MODE_CUSTOM;//mode;
      isLCD = 1;
	} else {
      driverId.nDrvInstID = 0; /* on chip HDMI */
      driverId.eDispVencMode = gst_omx_display_string_to_mode(self->display_mode);;
	  isLCD = 0;
	}

    err = OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId, &driverId);
    if(err != OMX_ErrorNone) 
        return FALSE;

#if 1
    if(isLCD) {
     _G_OMX_INIT_PARAM (&customModeInfo);
    
    customModeInfo.width = LCD_WIDTH;
    customModeInfo.height = LCD_HEIGHT;
    customModeInfo.scanFormat = OMX_SF_PROGRESSIVE;
    customModeInfo.pixelClock = LCD_PIXEL_CLOCK;
    customModeInfo.hFrontPorch = LCD_H_FRONT_PORCH;
    customModeInfo.hBackPorch = LCD_H_BACK_PORCH;
    customModeInfo.hSyncLen = LCD_H_SYNC_LENGTH;
    customModeInfo.vFrontPorch = LCD_V_FRONT_PORCH;
    customModeInfo.vBackPorch = LCD_V_BACK_PORCH;
    customModeInfo.vSyncLen = LCD_V_SYNC_LENGTH;
    /*Configure Display component and Display controller with these parameters*/

    err = OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE)
                               OMX_TI_IndexParamVFDCCustomModeInfo,
                               &customModeInfo); 
	if(err != OMX_ErrorNone) 
        return FALSE;
    }
#endif
    g_omx_core_change_state (gomx, OMX_StateIdle);
    g_omx_core_change_state (gomx, OMX_StateExecuting);

    self->mode_configured = TRUE;

    return TRUE;
}

static gboolean
start (GstBaseTransform * trans)
{
    GstOmxBaseCtrl *self;

    self = GST_OMX_BASE_CTRL (trans);

    g_omx_core_init (self->gomx);
    return TRUE;
}

static gboolean
stop (GstBaseTransform * trans)
{
    GstOmxBaseCtrl *self;

    self = GST_OMX_BASE_CTRL (trans);

	g_omx_core_stop (self->gomx);
    g_omx_core_unload (self->gomx);

    g_omx_core_free (self->gomx);

    return TRUE;
}

static GstFlowReturn
transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{   
    GstOmxBaseCtrl *self;
    GstFlowReturn ret = GST_FLOW_OK;

    self = GST_OMX_BASE_CTRL (trans);

    /* if mode is already configure then return */
    if (self->mode_configured)
        return ret;

    if (!gst_omx_ctrl_set_display_mode (self))
        ret = GST_FLOW_ERROR;

    return ret;
}

prepare_output_buffer (GstBaseTransform * trans,
  GstBuffer * in_buf, gint out_size, GstCaps * out_caps, GstBuffer ** out_buf)
{
    *out_buf = gst_buffer_ref (in_buf);
    return GST_FLOW_OK;
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseCtrl *self;

    self = GST_OMX_BASE_CTRL (obj);

    switch (prop_id)
    {
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
        case ARG_DISPLAY_MODE:
            g_free (self->display_mode);
            self->display_mode = g_value_dup_string (value);
            break;
		case ARG_DISPLAY_DEVICE:
            g_free (self->display_device);
            self->display_device = g_value_dup_string (value);
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
    GstOmxBaseCtrl *self;

    self = GST_OMX_BASE_CTRL (obj);

    switch (prop_id)
    {
        case ARG_COMPONENT_ROLE:
            g_value_set_string (value, self->omx_role);
            break;
        case ARG_COMPONENT_NAME:
            g_value_set_string (value, self->omx_component);
            break;
        case ARG_LIBRARY_NAME:
            g_value_set_string (value, self->omx_library);
            break;
        case ARG_DISPLAY_MODE:
            g_value_set_string (value, self->display_mode);
            break;
		case ARG_DISPLAY_DEVICE:
            g_value_set_string (value, self->display_device);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
finalize (GObject *obj)
{
    GstOmxBaseCtrl *self;

    self = GST_OMX_BASE_CTRL (obj);

    g_free (self->omx_role);
    g_free (self->omx_component);
    g_free (self->omx_library);

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;
    GstOmxBaseCtrl *gst_base_ctrl_class;
    GstOmxBaseCtrlClass *omx_base_class;

    gobject_class = G_OBJECT_CLASS (g_class);
    gst_base_ctrl_class = GST_OMX_BASE_CTRL (g_class);
    omx_base_class = GST_OMX_BASE_CTRL_CLASS (g_class);

    gobject_class->finalize = finalize;
    
    /* Properties stuff */
    {
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;

        g_object_class_install_property (gobject_class, ARG_COMPONENT_ROLE,
            g_param_spec_string ("component-role", "Component role",
            "Role of the OpenMAX IL component",
            NULL, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_COMPONENT_NAME,
            g_param_spec_string ("component-name", "Component name",
            "Name of the OpenMAX IL component to use",
             NULL, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_LIBRARY_NAME,
            g_param_spec_string ("library-name", "Library name",
            "Name of the OpenMAX IL implementation library to use",
             NULL, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_DISPLAY_MODE,
            g_param_spec_string ("display-mode", "Display mode", 
            "Display driver configuration mode (see below)"
            " \n\t\t\t OMX_DC_MODE_NTSC"
            " \n\t\t\t OMX_DC_MODE_PAL"
            " \n\t\t\t OMX_DC_MODE_1080P_60"
            " \n\t\t\t OMX_DC_MODE_720P_60" 
            " \n\t\t\t OMX_DC_MODE_1080I_60"
            " \n\t\t\t OMX_DC_MODE_1080P_30\n", "OMX_DC_MODE_1080P_60", G_PARAM_READWRITE));

		g_object_class_install_property (gobject_class, ARG_DISPLAY_DEVICE,
                                    g_param_spec_string ("display-device", "Display Device", 
            "Display device to be used -"
            "\n\t\t\t HDMI "
            "\n\t\t\t LCD ", "HDMI",G_PARAM_READWRITE));

    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{

    GstOmxBaseCtrl *self;
    GstOmxBaseCtrlClass *klass;
    GstBaseTransformClass   *trans_class;

    self = GST_OMX_BASE_CTRL (instance);
    klass = GST_OMX_BASE_CTRL_CLASS (g_class);
    trans_class = (GstBaseTransformClass *) klass;

    GST_LOG_OBJECT (self, "begin");

    self->gomx = g_omx_core_new (self, g_class);
    self->in_port = g_omx_core_get_port (self->gomx, "in", 0);
    
    trans_class->passthrough_on_same_caps = TRUE;
    trans_class->transform_ip = GST_DEBUG_FUNCPTR (transform_ip);
    trans_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (prepare_output_buffer);
    trans_class->start = GST_DEBUG_FUNCPTR (start);
    trans_class->stop = GST_DEBUG_FUNCPTR (stop);

    g_object_set (self, "display-mode", "OMX_DC_MODE_1080P_60", NULL);
	g_object_set (self, "display-device", "HDMI", NULL);

    GST_LOG_OBJECT (self, "end");
}

