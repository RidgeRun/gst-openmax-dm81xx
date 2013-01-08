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

#include "gstomx_mpeg4dec.h"
#include "gstomx.h"

GSTOMX_BOILERPLATE (GstOmxMpeg4Dec, gst_omx_mpeg4dec, GstOmxBaseVideoDec, GST_OMX_BASE_VIDEODEC_TYPE);

static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

    struc = gst_structure_new ("video/mpeg",
                               "mpegversion", G_TYPE_INT, 4,
                               "systemstream", G_TYPE_BOOLEAN, FALSE,
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    gst_caps_append_structure (caps, struc);

    struc = gst_structure_new ("video/x-divx",
                               "divxversion", GST_TYPE_INT_RANGE, 4, 5,
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    gst_caps_append_structure (caps, struc);

    struc = gst_structure_new ("video/x-xvid",
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    gst_caps_append_structure (caps, struc);

    struc = gst_structure_new ("video/x-3ivx",
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

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

        details.longname = "OpenMAX IL MPEG-4 video decoder";
        details.klass = "Codec/Decoder/Video";
        details.description = "Decodes video in MPEG-4 format with OpenMAX IL";
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
#define UTIL_ALIGN(a,b)  ((((guint32)(a)) + (b)-1) & (~((guint32)((b)-1))))
//#define     PADX    16
//#define     PADY    16

static void
initialize_port (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVideoDec *self;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
    gint width, height;
    GOmxPort *port;
	OMX_PORT_PARAM_TYPE portInit;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    width = self->extendedParams.width;
    height = self->extendedParams.height;
	printf("Width :%d, Height: %d\n",width,height);
	
	_G_OMX_INIT_PARAM (&portInit);
	
	  portInit.nPorts = 2;
	  portInit.nStartPortNumber = 0;
	  //OMX_SetParameter (pHandle, OMX_IndexParamVideoInit, &portInit);
	  G_OMX_PORT_SET_PARAM(omx_base->in_port, OMX_IndexParamVideoInit, &portInit);
	  
	#if 1 
    GST_DEBUG_OBJECT (self, "G_OMX_PORT_GET_DEFINITION (output)");
	_G_OMX_INIT_PARAM (&pInPortDef);
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
  pInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  /* this is codec setting, OMX component does not support it */
  pInPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
  /* color format is irrelavant */
  pInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &pInPortDef);	
#endif
#if 1
    _G_OMX_INIT_PARAM (&pOutPortDef);
    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &pOutPortDef);
    pOutPortDef.nPortIndex = 1;
  pOutPortDef.eDir = OMX_DirOutput;
  /* componet would expect these numbers of buffers to be allocated */
  //pOutPortDef.nBufferCountActual = 8;
  pOutPortDef.nBufferCountMin = 1;

  /* Codec requires padded height and width and width needs to be aligned at
     128 byte boundary */
  pOutPortDef.nBufferSize =
    (UTIL_ALIGN ((width + (2 * 16)), 128) * ((height + (4 * 16))) * 3) >> 1;

  pOutPortDef.bEnabled = OMX_TRUE;
  pOutPortDef.bPopulated = OMX_FALSE;
  pOutPortDef.eDomain = OMX_PortDomainVideo;
  /* currently component alloactes contigous buffers with 128 alignment, these
     values are do't care */
  pOutPortDef.bBuffersContiguous = OMX_FALSE;
  pOutPortDef.nBufferAlignment = 0x0;

  /* OMX_VIDEO_PORTDEFINITION values for output port */
  pOutPortDef.format.video.cMIMEType = "H264";
  pOutPortDef.format.video.pNativeRender = NULL;
  pOutPortDef.format.video.nFrameWidth = width;
  pOutPortDef.format.video.nFrameHeight = height;

  /* stride is set as buffer width */
  pOutPortDef.format.video.nStride = UTIL_ALIGN (width + (2 * 16), 128);
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
#if 0
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
#endif
    GST_INFO_OBJECT (omx_base, "end");
}


static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseVideoDec *omx_base;

    omx_base = GST_OMX_BASE_VIDEODEC (instance);

    omx_base->compression_format = OMX_VIDEO_CodingMPEG4;
	omx_base->initialize_port = initialize_port;
}
