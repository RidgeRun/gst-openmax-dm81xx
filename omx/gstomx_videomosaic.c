/*
 * Copyright (C) 2013 Vodalys Ingénierie
 *
 * Author: Jean-Baptiste Théou <jean-baptiste.theou@vodalys.com>
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

/* Video Mixer plugin, based on OMX.TI.VPSSM3.VSWMOSAIC OpenMax IL component
 * 
 * */

#include "gstomx_videomosaic.h"
#include "gstomx.h"

#define CAMERA_INPUT_IDX 0
#define SLIDE_INPUT_IDX 1

#define BLACK 0x80008000
#define GRAY  0x80808080
#define GREEN 0x00800080

#define BG_COLOR BLACK

GSTOMX_BOILERPLATE (GstOmxVideoMosaic, gst_omx_video_mosaic, GstOmxBaseFilter21, GST_OMX_BASE_FILTER21_TYPE);


static GstStaticPadTemplate sink_template_camera =
        GST_STATIC_PAD_TEMPLATE ("sink_00",
                GST_PAD_SINK,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ( "{YUY2}" ))
        );

static GstStaticPadTemplate sink_template_slide =
        GST_STATIC_PAD_TEMPLATE ("sink_01",
                GST_PAD_SINK,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ( "{YUY2}" ))
        );

static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE ("src",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ( "{YUY2}" ))
        );


static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL videomixer";
        details.klass = "Filter";
        details.description = "Video Mixer element";
        details.author = "Jean-Baptiste Theou";

        gst_element_class_set_details (element_class, &details);
    }

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template_camera));

	gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template_slide));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
}


static GstCaps * create_src_caps (GstOmxBaseFilter21 *omx_base)
{
    GstCaps *caps;    
    GstOmxBaseFilter21 *self;
    int width, height;
    GstStructure *struc;

    self = GST_OMX_BASE_FILTER21 (omx_base);
    caps = gst_pad_peer_get_caps (omx_base->srcpad);

    if (gst_caps_is_empty (caps))
    {

        width = 1920;
        height = 1088;

    }
    else
    {
        GstStructure *s;

        s = gst_caps_get_structure (caps, 0);

        if (!(gst_structure_get_int (s, "width", &width) &&
            gst_structure_get_int (s, "height", &height)))
        {

			width = 1920;
			height = 1088;

        }
    }

    caps = gst_caps_new_empty ();
    struc = gst_structure_new (("video/x-raw-yuv"),
            "width",  G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
            NULL);


    gst_caps_append_structure (caps, struc);

    return caps;
}


static void
omx_setup (GstOmxBaseFilter21 *omx_base)
{
	int i;
	GOmxCore *gomx;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
	OMX_PARAM_PORTDEFINITIONTYPE paramPort;
	OMX_CONFIG_VSWMOSAIC_CREATEMOSAICLAYOUT  sMosaic;
	OMX_CONFIG_VSWMOSAIC_SETBACKGROUNDCOLOR  sMosaicBg;
	OMX_PARAM_VSWMOSAIC_MOSAIC_PERIODICITY	 sMosaicFPS;
    GstOmxBaseFilter21 *self;
    gomx = (GOmxCore *) omx_base->gomx;
    self = GST_OMX_BASE_FILTER21(omx_base);
	
    GST_LOG_OBJECT (self, "begin");
    /* set the output cap */
    gst_pad_set_caps (self->srcpad, create_src_caps (omx_base));
    /* Setting Memory type at input port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting input port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
	memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;

	for ( i = 0; i < NUM_INPUTS; i++) {
		memTypeCfg.nPortIndex = omx_base->in_port[i]->port_index;
		eError = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);
		if (eError != OMX_ErrorNone)
		{
				return;
		}
	}
	
	/* Setting Memory type at output port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting output port to Raw memory");
    
    memTypeCfg.nPortIndex = omx_base->out_port->port_index;
	memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
	eError = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

	if (eError != OMX_ErrorNone)
	{
		return;
	}

	GST_LOG_OBJECT (self, "Setting port definition (input)");


	for ( i = 0; i < NUM_INPUTS; i++) {
		/* set input height/width and color format */
		G_OMX_PORT_GET_DEFINITION (omx_base->in_port[i], &paramPort);
		paramPort.format.video.nFrameWidth  = self->in_width[i]; 
		paramPort.format.video.nFrameHeight = self->in_height[i];
		/* swmosaic is connceted to scalar, whose stride is different than width*/
		paramPort.format.video.nStride = self->in_stride[i];
		paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
		paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
		paramPort.nBufferSize = (paramPort.format.video.nStride * paramPort.format.video.nFrameHeight);

		paramPort.nBufferAlignment = 0;
		paramPort.bBuffersContiguous = 0;
	
		G_OMX_PORT_SET_DEFINITION (omx_base->in_port[i], &paramPort);
		g_omx_port_setup (omx_base->in_port[i], &paramPort);
	}
	
	G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &paramPort);
	paramPort.format.video.nFrameWidth  = self->out_width;
	paramPort.format.video.nFrameHeight = self->out_height;
	paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
	paramPort.nBufferAlignment = 0;
	paramPort.bBuffersContiguous = 0;
	paramPort.nBufferCountActual = 4;
	paramPort.format.video.nStride = self->out_stride;
	paramPort.nBufferSize = paramPort.format.video.nStride * paramPort.format.video.nFrameHeight;
	
	G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &paramPort);
	g_omx_port_setup (omx_base->out_port, &paramPort);
	
	_G_OMX_INIT_PARAM (&sMosaic);
	sMosaic.nPortIndex  = OMX_VSWMOSAIC_OUTPUT_PORT_START_INDEX;
  
	sMosaic.nOpWidth    = self->out_width;  /* Width in pixels  */
	sMosaic.nOpHeight   = self->out_height; /* Height in pixels */
	sMosaic.nOpPitch    = self->out_stride; /* Pitch in bytes   */

	sMosaic.nNumWindows = 2;
	sMosaic.sMosaicWinFmt[0].dataFormat = OMX_COLOR_FormatYCbYCr;
	sMosaic.sMosaicWinFmt[0].nPortIndex = 0;
	sMosaic.sMosaicWinFmt[0].pitch[0]   =  self->in_stride[0];
	sMosaic.sMosaicWinFmt[0].winStartX  = self->x[0];
	sMosaic.sMosaicWinFmt[0].winStartY  = self->y[0];
	sMosaic.sMosaicWinFmt[0].winWidth   = self->in_width[0];
	sMosaic.sMosaicWinFmt[0].winHeight  = self->in_height[0];
	
	sMosaic.sMosaicWinFmt[1].dataFormat = OMX_COLOR_FormatYCbYCr;
	sMosaic.sMosaicWinFmt[1].nPortIndex = 1;
	sMosaic.sMosaicWinFmt[1].pitch[0]   = self->in_stride[1];
	sMosaic.sMosaicWinFmt[1].winStartX  = self->x[1];
	sMosaic.sMosaicWinFmt[1].winStartY  = self->y[1];
	sMosaic.sMosaicWinFmt[1].winWidth   = self->in_width[1];
	sMosaic.sMosaicWinFmt[1].winHeight  = self->in_height[1];
	
	eError = OMX_SetConfig (gomx->omx_handle, 
							OMX_TI_IndexConfigVSWMOSAICCreateMosaicLayout, 
							&sMosaic);
	if (eError != OMX_ErrorNone)
	{
			printf("Error during Layout settings\n");
	}
	_G_OMX_INIT_PARAM(&sMosaicBg);
	sMosaicBg.nPortIndex  = OMX_VSWMOSAIC_OUTPUT_PORT_START_INDEX;
	sMosaicBg.uColor = BG_COLOR;
	eError = OMX_SetConfig (gomx->omx_handle, 
							OMX_TI_IndexConfigVSWMOSAICSetBackgroundColor, 
							&sMosaicBg);
	if (eError != OMX_ErrorNone)
	{
			printf("Error during background\n");
	}
	
// FPS settings didn't work actually 
#if 1
	_G_OMX_INIT_PARAM(&sMosaicFPS);
	sMosaicFPS.nPortIndex  = OMX_VSWMOSAIC_OUTPUT_PORT_START_INDEX;
	sMosaicFPS.nFps		   = 60;				
	eError = OMX_SetConfig (gomx->omx_handle, 
							OMX_TI_IndexParamVSWMOSAICMosaicPeriodicity, 
							&sMosaicFPS);
	if (eError != OMX_ErrorNone)
	{
			printf("Error during FPS settings : %s\n",g_omx_error_to_str (eError));
	}
#endif
}


static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter21 *omx_base;

    omx_base = GST_OMX_BASE_FILTER21 (instance);
	omx_base->omx_setup = omx_setup;
    GST_DEBUG_OBJECT (omx_base, "start");
}
