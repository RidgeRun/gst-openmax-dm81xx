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

#include "gstomx_videosink.h"
#include "gstomx_base_sink.h"
#include "gstomx.h"

#include <string.h> /* for memset, strcmp */

#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfdc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/ti81xxfb.h>
#include <unistd.h>

GSTOMX_BOILERPLATE (GstOmxVideoSink, gst_omx_videosink, GstOmxBaseSink, GST_OMX_BASE_SINK_TYPE);

enum
{
    ARG_0,
    ARG_X_SCALE,
    ARG_Y_SCALE,
    ARG_ROTATION,
    ARG_TOP,
    ARG_LEFT,
    ARG_DISPLAY_MODE,
    ARG_ENABLE_COLORKEY,
    ARG_DISPLAY_DEVICE,
};

static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

    struc = gst_structure_new ("video/x-raw-yuv",
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    {
        GValue list;
        GValue val;

        list.g_type = val.g_type = 0;

        g_value_init (&list, GST_TYPE_LIST);
        g_value_init (&val, GST_TYPE_FOURCC);
        gst_value_set_fourcc (&val, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'));
        gst_value_list_append_value (&list, &val);
        gst_structure_set_value (struc, "format", &list);

        g_value_unset (&val);
        g_value_unset (&list);
    }

    gst_caps_append_structure (caps, struc);

    return caps;
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL videosink element";
        details.klass = "Video/Sink";
        details.description = "Renders video";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    {
        GstPadTemplate *template;

        template = gst_pad_template_new ("sink", GST_PAD_SINK,
                                         GST_PAD_ALWAYS,
                                         generate_sink_template ());

        gst_element_class_add_pad_template (element_class, template);
    }
}

static int
gstomx_videosink_colorkey (gboolean enable)
{
	struct fb_fix_screeninfo fixinfo;
	struct fb_var_screeninfo varinfo;
    int fd = 0, ret = -1;
    struct ti81xxfb_region_params regp;

	fd = open("/dev/fb0", O_RDWR);

	if (fd <= 0) {
        return -1;
	}

	ret = ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo);
	if (ret < 0) {        
        goto exit;
	}
    
	ret = ioctl(fd, FBIOGET_VSCREENINFO, &varinfo);
	if (ret < 0) {
        goto exit;
	}

	ret = ioctl(fd, TIFB_GET_PARAMS, &regp);
	if (ret < 0) {
        goto exit;
	}

    regp.transen = enable ? TI81XXFB_FEATURE_ENABLE : TI81XXFB_FEATURE_DISABLE;
    regp.transcolor = 0x000000;

	ret = ioctl(fd, TIFB_SET_PARAMS, &regp);
	if (ret < 0) {
        goto exit;
	}

    ret = 0;
exit:
    if (fd)
        close(fd);

    return ret;
}

static int
get_display_mode_from_string (char *str, int *mode, int *maxWidth, int *maxHeight)
{
    if (!strcmp (str, "OMX_DC_MODE_1080P_30"))
    {
        *mode = OMX_DC_MODE_1080P_30;
        *maxWidth = 1920;
        *maxHeight = 1080;
        return;
    }
    
    if (!strcmp (str, "OMX_DC_MODE_1080I_60"))
    {
        *mode = OMX_DC_MODE_1080I_60;
        *maxWidth = 1920;
        *maxHeight = 1080;
        return;
    }

    if (!strcmp (str, "OMX_DC_MODE_720P_60"))
    {
        *mode = OMX_DC_MODE_720P_60;
        *maxWidth = 1280;
        *maxHeight = 720;
        return;
    }

    if (!strcmp (str, "OMX_DC_MODE_1080P_60"))
    {
        *mode = OMX_DC_MODE_1080P_60;
        *maxWidth = 1920;
        *maxHeight = 1080;
        return;
    }
   
    if (!strcmp (str, "OMX_DC_MODE_PAL"))
    {
        *mode = OMX_DC_MODE_PAL;
        *maxWidth = 720;
        *maxHeight = 576;
        return;
    }

    if (!strcmp (str, "OMX_DC_MODE_NTSC"))
    {
        *mode = OMX_DC_MODE_NTSC;
        *maxWidth = 720;
        *maxHeight = 480;
        return;
    }

    return;
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


static void
omx_setup (GstBaseSink *gst_sink, GstCaps *caps)
{
    GstOmxBaseSink *omx_base, *self;
    GstOmxVideoSink *sink;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE param;
    OMX_PARAM_VFDC_DRIVERINSTID driverId;
    OMX_PARAM_VFDC_CREATEMOSAICLAYOUT mosaicLayout;
    OMX_CONFIG_VFDC_MOSAICLAYOUT_PORT2WINMAP port2Winmap;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
	OMX_PARAM_VFDC_FIELD_MERGE_INFO fieldMergeInfo;
	OMX_PARAM_DC_CUSTOM_MODE_INFO customModeInfo;
    GstStructure *structure;
    gint width;
    gint height;
    gint maxWidth, maxHeight, mode;
	guint isLCD;

    sink = GST_OMX_VIDEOSINK (gst_sink);
    self = omx_base = GST_OMX_BASE_SINK (gst_sink);
    gomx = (GOmxCore *) omx_base->gomx;

    if (self->port_initialized)
        return;

    structure = gst_caps_get_structure (caps, 0);

    gst_structure_get_int (structure, "width", &width);
    gst_structure_get_int (structure, "height", &height);

    /* set input port definition */
    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

    param.nBufferSize = (width * height * 2);
    param.format.video.nFrameWidth = width;
    param.format.video.nFrameHeight = height;
	param.format.video.nStride    = width * 2;
    param.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    param.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    param.nBufferCountActual = 5;

    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
    g_omx_port_setup (omx_base->in_port, &param);

    /* get the display mode set via property */
    get_display_mode_from_string (sink->display_mode, &mode, &maxWidth, &maxHeight);

    /* set display driver mode */
    _G_OMX_INIT_PARAM (&driverId);
	
	if(!strcmp(sink->display_device,"LCD")) {
      driverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD1;
      driverId.eDispVencMode = OMX_DC_MODE_CUSTOM;//mode;
      isLCD = 1;
	} else {
      driverId.nDrvInstID = 0; /* on chip HDMI */
      driverId.eDispVencMode = mode;
	  isLCD = 0;
	}

    OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId, &driverId);

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
	
		OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE)
								   OMX_TI_IndexParamVFDCCustomModeInfo,
								   &customModeInfo); 
    }
#endif

    /* center the video */
    if (!sink->left && !sink->top)
    {
        sink->left = ((maxWidth - width) / 2) & ~1;         
        sink->top = ((maxHeight - height) / 2) & ~1;
    }

    /* set mosiac window information */
    _G_OMX_INIT_PARAM (&mosaicLayout);
    mosaicLayout.nPortIndex = 0;
	
  if (!isLCD) {
      mosaicLayout.sMosaicWinFmt[0].winStartX = sink->left;
      mosaicLayout.sMosaicWinFmt[0].winStartY = sink->top;
      mosaicLayout.sMosaicWinFmt[0].winWidth = width;
      mosaicLayout.sMosaicWinFmt[0].winHeight = height;
      mosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] = width * 2;
  } else {
      /* For LCD Display, start the window at (0,0) */
      mosaicLayout.sMosaicWinFmt[0].winStartX = 0;
      mosaicLayout.sMosaicWinFmt[0].winStartY = 0;
      
      /*If LCD is chosen, fir the mosaic window to the size of the LCD display*/
      mosaicLayout.sMosaicWinFmt[0].winWidth = LCD_WIDTH;
      mosaicLayout.sMosaicWinFmt[0].winHeight = LCD_HEIGHT;
      mosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] = 
                                         LCD_WIDTH * 2;  
  	}
    mosaicLayout.sMosaicWinFmt[0].dataFormat =  VFDC_DF_YUV422I_YVYU;
    mosaicLayout.sMosaicWinFmt[0].bpp = VFDC_BPP_BITS16;
    mosaicLayout.sMosaicWinFmt[0].priority = 0;
    mosaicLayout.nDisChannelNum = 0;
    mosaicLayout.nNumWindows = 1;

    OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCCreateMosaicLayout, 
        &mosaicLayout);

    /* set port to window mapping */
    _G_OMX_INIT_PARAM (&port2Winmap);
    port2Winmap.nLayoutId = 0; 
    port2Winmap.numWindows = 1; 
    port2Winmap.omxPortList[0] = OMX_VFDC_INPUT_PORT_START_INDEX + 0;

    OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigVFDCMosaicPort2WinMap, &port2Winmap);

    /* set default input memory to Raw */
    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = 0;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    
    OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

	_G_OMX_INIT_PARAM (&fieldMergeInfo);
    fieldMergeInfo.fieldMergeMode = FALSE;
    
    OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCFieldMergeMode, &fieldMergeInfo);
 
    /* enable the input port */
    OMX_SendCommand (gomx->omx_handle, OMX_CommandPortEnable, omx_base->in_port->port_index, NULL);
    g_sem_down (omx_base->in_port->core->port_sem);

    /* port is now initalized */
    self->port_initialized = TRUE;

    return;
}


static gboolean
setcaps (GstBaseSink *gst_sink,
         GstCaps *caps)
{
    GstOmxBaseSink *omx_base;
    GstOmxVideoSink *self;
    GOmxCore *gomx;

    omx_base = GST_OMX_BASE_SINK (gst_sink);
    self = GST_OMX_VIDEOSINK (gst_sink);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

    omx_setup (gst_sink, caps);

    return TRUE;
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxVideoSink *self;

    self = GST_OMX_VIDEOSINK (object);

    switch (prop_id)
    {
        case ARG_X_SCALE:
            self->x_scale = g_value_get_uint (value);
            break;
        case ARG_Y_SCALE:
            self->y_scale = g_value_get_uint (value);
            break;
        case ARG_ROTATION:
            self->rotation = g_value_get_uint (value);
            break;
        case ARG_TOP:
            self->top = g_value_get_uint (value);
            break;
        case ARG_LEFT:
            self->left = g_value_get_uint (value);
            break;
        case ARG_ENABLE_COLORKEY:
            self->colorkey = g_value_get_boolean (value);
            gstomx_videosink_colorkey (self->colorkey);
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
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    GstOmxVideoSink *self;

    self = GST_OMX_VIDEOSINK (object);

    switch (prop_id)
    {
        case ARG_X_SCALE:
            g_value_set_uint (value, self->x_scale);
            break;
        case ARG_Y_SCALE:
            g_value_set_uint (value, self->y_scale);
            break;
        case ARG_ROTATION:
            g_value_set_uint (value, self->rotation);
            break;
        case ARG_LEFT:
            g_value_set_uint (value, self->left);
            break;
        case ARG_TOP:
            g_value_set_uint (value, self->top);
            break;
        case ARG_ENABLE_COLORKEY:
            g_value_set_boolean (value, self->colorkey);
            break;
        case ARG_DISPLAY_MODE:
            g_value_set_string (value, self->display_mode);
            break;
		case ARG_DISPLAY_DEVICE:
            g_value_set_string (value, self->display_device);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;
    GstBaseSinkClass *gst_base_sink_class;

    gobject_class = (GObjectClass *) g_class;
    gst_base_sink_class = GST_BASE_SINK_CLASS (g_class);

    gst_base_sink_class->set_caps = setcaps;

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    g_object_class_install_property (gobject_class, ARG_TOP,
                                     g_param_spec_uint ("top", "Top",
                                                        "The top most co-ordinate on video display",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_LEFT,
                                     g_param_spec_uint ("left", "left",
                                                        "The left most co-ordinate on video display",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_ENABLE_COLORKEY,
                                     g_param_spec_boolean ("colorkey", "Enable colorkey",
                                                        "Enable colorkey",
                                                        TRUE, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_DISPLAY_MODE,
                                    g_param_spec_string ("display-mode", "Display mode", 
            "Display driver configuration mode (see below)"
            "\n\t\t\t OMX_DC_MODE_NTSC "
            "\n\t\t\t OMX_DC_MODE_PAL "
            "\n\t\t\t OMX_DC_MODE_1080P_60 "
            "\n\t\t\t OMX_DC_MODE_720P_60 "
            "\n\t\t\t OMX_DC_MODE_1080I_60 "
            "\n\t\t\t OMX_DC_MODE_1080P_30", 
            "OMX_DC_MODE_1080P_60", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_DISPLAY_DEVICE,
                                    g_param_spec_string ("display-device", "Display Device", 
            "Display device to be used -"
            "\n\t\t\t HDMI "
            "\n\t\t\t LCD ", "HDMI",G_PARAM_READWRITE));
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseSink *omx_base;
    GstOmxVideoSink *self;

    omx_base = GST_OMX_BASE_SINK (instance);
    self = GST_OMX_VIDEOSINK (instance);
    omx_base->omx_setup = omx_setup;    
    
    g_object_set (self, "colorkey", TRUE, NULL);
    g_object_set (self, "display-mode", "OMX_DC_MODE_1080P_60", NULL);
	g_object_set (self, "display-device", "HDMI", NULL);
}

