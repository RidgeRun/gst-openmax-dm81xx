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

#include "gstomx_mjpegdec.h"
#include "gstomx.h"

GSTOMX_BOILERPLATE (GstOmxMJPEGDec, gst_omx_mjpegdec, GstOmxBaseVideoDec, GST_OMX_BASE_VIDEODEC_TYPE);

static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

/* 
image/jpeg
format: { I420, Y41B, UYVY, YV12 }
width: [ 0, 2147483647 ]
height: [ 0, 2147483647 ]
interlaced: { true, false }
framerate: [ 0/1, 2147483647/1 ]
parsed: true
*/
    struc = gst_structure_new ("image/jpeg",
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

        details.longname = "OpenMAX IL JPEG/MJPEG decoder";
        details.klass = "Codec/Decoder/Video";
        details.description = "Decodes video/images in MJPEG/JPEG format with OpenMAX IL";
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

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
}

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
 
    GST_DEBUG_OBJECT (self, "G_OMX_PORT_GET_DEFINITION (output)");
    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &paramPort);

    width = self->extendedParams.width;
    height = self->extendedParams.height;

    paramPort.nPortIndex = 1;
	paramPort.nBufferCountActual = 20;//15;//output_buffer_count
    paramPort.format.video.nFrameWidth = width;
    paramPort.format.video.nFrameHeight = height;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
	paramPort.format.video.xFramerate = (60) << 16;

    GST_DEBUG_OBJECT (self, "nFrameWidth = %ld, nFrameHeight = %ld, nBufferCountActual = %ld",
      paramPort.format.video.nFrameWidth, paramPort.format.video.nFrameHeight, 
      paramPort.nBufferCountActual);

    GST_DEBUG_OBJECT (self, "G_OMX_PORT_SET_DEFINITION (output)");
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &paramPort);

	G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &paramPort);
    //paramPort.nBufferCountActual = 8;
    paramPort.format.video.xFramerate = (60) << 16;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &paramPort);
#if 1
    port = g_omx_core_get_port (gomx, "input", 0);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    port = g_omx_core_get_port (gomx, "output", 1);

    GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);
#endif
    GST_INFO_OBJECT (omx_base, "end");
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseVideoDec *omx_base;

    omx_base = GST_OMX_BASE_VIDEODEC (instance);

    omx_base->compression_format = OMX_VIDEO_CodingMJPEG;
    omx_base->initialize_port = initialize_port;
}
