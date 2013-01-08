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

#include "gstomx_mpeg2dec.h"
#include "gstomx.h"

GSTOMX_BOILERPLATE (GstOmxMpeg2Dec, gst_omx_mpeg2dec, GstOmxBaseVideoDec, GST_OMX_BASE_VIDEODEC_TYPE);

#if 0
static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

    struc = gst_structure_new ("video/mpeg",
							   "mpegversion",G_TYPE_INT,2,
							   "systemstream",G_TYPE_BOOLEAN,FALSE,
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                              /* "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,*/
                               NULL);

    gst_caps_append_structure (caps, struc);

    return caps;
}
#else
static GstStaticPadTemplate mpeg2dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/mpeg, "
			"mpegversion = (int) [ 1, 2 ], "
			"parsed = (boolean) true, "
			"systemstream = (boolean) false, "
			"width = (int) [ 16, 4096 ], "
			"height = (int) [ 16, 4096 ]")
		);
#endif
static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);
	{
#if 0
		GstPadTemplate *template;

		template = gst_pad_template_new ("sink", GST_PAD_SINK,
				GST_PAD_ALWAYS,
				generate_sink_template ());

		gst_element_class_add_pad_template (element_class, template);
#else
		gst_element_class_add_pad_template (element_class, 
				gst_static_pad_template_get (&mpeg2dec_sink_template));
#endif
	}
    {
        GstElementDetails details;

        details.longname = "OpenMAX IL MPEG2 video decoder";
        details.klass = "Codec/Decoder/Video";
        details.description = "Decodes video in MPEG2 format with OpenMAX IL";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
}
#if 0
static void
initialize_port (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVideoDec *self;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE paramPort;
    gint width, height;
    GOmxPort *port;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");
#if 1 
    GST_DEBUG_OBJECT (self, "G_OMX_PORT_GET_DEFINITION (output)");
    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &paramPort);

    width = self->extendedParams.width;
    height = self->extendedParams.height;

    paramPort.nPortIndex = 1;
	paramPort.nBufferCountActual = 6;//output_buffer_count
    paramPort.format.video.nFrameWidth = width;
    paramPort.format.video.nFrameHeight = height;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

    GST_DEBUG_OBJECT (self, "nFrameWidth = %ld, nFrameHeight = %ld, nBufferCountActual = %ld",
      paramPort.format.video.nFrameWidth, paramPort.format.video.nFrameHeight, 
      paramPort.nBufferCountActual);

    GST_DEBUG_OBJECT (self, "G_OMX_PORT_SET_DEFINITION (output)");
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &paramPort);
#endif
    port = g_omx_core_get_port (gomx, "in", 0);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    port = g_omx_core_get_port (gomx, "out", 1);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    GST_INFO_OBJECT (omx_base, "end");
}
#else
#define UTIL_ALIGN(a,b)  ((((guint32)(a)) + (b)-1) & (~((guint32)((b)-1))))
#define     PADX    32
#define     PADY    24

static void
initialize_port (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVideoDec *self;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
    gint width, height;
    GOmxPort *port;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");
#if 1 
    width = self->extendedParams.width;
    height = self->extendedParams.height;
	printf("Width :%d, Height: %d\n",width,height);
    GST_DEBUG_OBJECT (self, "G_OMX_PORT_GET_DEFINITION (output)");
    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &pInPortDef);
    pInPortDef.nPortIndex = 0;
  /* It is input port so direction is set as Input, Empty buffers call would be 
     accepted based on this */
  pInPortDef.eDir = OMX_DirInput;
  /* number of buffers are set here */
  pInPortDef.nBufferCountActual = 4;
  pInPortDef.nBufferCountMin = 1;
  /* buffer size by deafult is assumed as width * height for input bitstream
     which would suffice most of the cases */
  pInPortDef.nBufferSize = width * height;

  pInPortDef.bEnabled = OMX_TRUE;
  pInPortDef.bPopulated = OMX_FALSE;
  pInPortDef.eDomain = OMX_PortDomainVideo;
  pInPortDef.bBuffersContiguous = OMX_FALSE;
  pInPortDef.nBufferAlignment = 0x0;

  /* OMX_VIDEO_PORTDEFINITION values for input port */
  pInPortDef.format.video.cMIMEType = "H264";
  pInPortDef.format.video.pNativeRender = NULL;
  /* set the width and height, used for buffer size calculation */
  pInPortDef.format.video.nFrameWidth = width;
  pInPortDef.format.video.nFrameHeight = height;
  /* for bitstream buffer stride is not a valid parameter */
  pInPortDef.format.video.nStride = -1;
  /* component supports only frame based processing */
  pInPortDef.format.video.nSliceHeight = 0;

  /* bitrate does not matter for decoder */
  pInPortDef.format.video.nBitrate = 104857600;
  /* as per openmax frame rate is in Q16 format */
  pInPortDef.format.video.xFramerate = 60 << 16;
  /* input port would receive H264 stream */
  pInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
  /* this is codec setting, OMX component does not support it */
  pInPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
  /* color format is irrelavant */
  pInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &pInPortDef);	
	
    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &pOutPortDef);
    pOutPortDef.nPortIndex = 1;
  pOutPortDef.eDir = OMX_DirOutput;
  /* componet would expect these numbers of buffers to be allocated */
  //pOutPortDef.nBufferCountActual = 8;
  pOutPortDef.nBufferCountMin = 1;

  /* Codec requires padded height and width and width needs to be aligned at
     128 byte boundary */
  pOutPortDef.nBufferSize =
    (UTIL_ALIGN ((width + (2 * PADX)), 128) * ((height + (4 * PADY))) * 3) >> 1;

  pOutPortDef.bEnabled = OMX_TRUE;
  pOutPortDef.bPopulated = OMX_FALSE;
  pOutPortDef.eDomain = OMX_PortDomainVideo;
  /* currently component alloactes contigous buffers with 128 alignment, these
     values are do't care */
  pOutPortDef.bBuffersContiguous = OMX_FALSE;
  pOutPortDef.nBufferAlignment = 0x0;

  /* OMX_VIDEO_PORTDEFINITION values for output port */
  pOutPortDef.format.video.cMIMEType = "MPEG2";
  pOutPortDef.format.video.pNativeRender = NULL;
  pOutPortDef.format.video.nFrameWidth = width;
  pOutPortDef.format.video.nFrameHeight = height;

  /* stride is set as buffer width */
  pOutPortDef.format.video.nStride = UTIL_ALIGN (width + (2 * PADX), 128);
  pOutPortDef.format.video.nSliceHeight = 0;

  /* bitrate does not matter for decoder */
  pOutPortDef.format.video.nBitrate = 25000000;
  /* as per openmax frame rate is in Q16 format */
  //pOutPortDef.format.video.xFramerate = 60 << 16;
  pOutPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
  /* output is raw YUV 420 SP format, It support only this */
  pOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  pOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

    /*GST_DEBUG_OBJECT (self, "nFrameWidth = %ld, nFrameHeight = %ld, nBufferCountActual = %ld",
      paramPort.format.video.nFrameWidth, paramPort.format.video.nFrameHeight, 
      paramPort.nBufferCountActual);*/

    GST_DEBUG_OBJECT (self, "G_OMX_PORT_SET_DEFINITION (output)");
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &pOutPortDef);
#endif
    port = g_omx_core_get_port (gomx, "in", 0);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    port = g_omx_core_get_port (gomx, "out", 1);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    GST_INFO_OBJECT (omx_base, "end");
}


#endif
static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseVideoDec *omx_base;

    omx_base = GST_OMX_BASE_VIDEODEC (instance);

    omx_base->compression_format = OMX_VIDEO_CodingMPEG2;
    omx_base->initialize_port = initialize_port;
}
