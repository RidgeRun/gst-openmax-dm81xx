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

#include "gstomx_scaler.h"
#include "gstomx.h"

GSTOMX_BOILERPLATE (GstOmxScaler, gst_omx_scaler, GstOmxBaseVfpc, GST_OMX_BASE_VFPC_TYPE);

static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE ("sink",
                GST_PAD_SINK,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (
                        "{NV12}", "[ 0, max ]"))
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

        details.longname = "OpenMAX IL for OMX.TI.VPSSM3.VFPC.INDTXSCWB component";
        details.klass = "Filter";
        details.description = "Scale video using VPSS Scaler module ";
        details.author = "Brijesh Singh";

        gst_element_class_set_details (element_class, &details);
    }

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));
}

static GstCaps*
create_src_caps (GstOmxBaseFilter *omx_base)
{
    GstCaps *caps;    
    GstOmxBaseVfpc *self;
    int width, height;
    GstStructure *struc;

    self = GST_OMX_BASE_VFPC (omx_base);
    caps = gst_pad_peer_get_caps (omx_base->srcpad);

    if (NULL == caps || gst_caps_is_empty (caps))
    {
        width = self->in_width;
        height = self->in_height;
    }
    else
    {
        GstStructure *s;

        s = gst_caps_get_structure (caps, 0);

        if (!(gst_structure_get_int (s, "width", &width) &&
            gst_structure_get_int (s, "height", &height)))
        {
            width = self->in_width;
            height = self->in_height;    
        }
    }
	/* Workaround: Make width multiple of 16, otherwise, scaler crashes */
	width = (width+15) & 0xFFFFFFF0;

    caps = gst_caps_new_empty ();
    struc = gst_structure_new (("video/x-raw-yuv"),
            "width",  G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
            NULL);


    if (self->framerate_denom)
    {
        gst_structure_set (struc,
        "framerate", GST_TYPE_FRACTION, self->framerate_num, self->framerate_denom, NULL);
    }

	if (self->pixel_aspect_ratio_denom)
	{
		gst_structure_set (struc,
				"pixel-aspect-ratio", GST_TYPE_FRACTION, self->pixel_aspect_ratio_num, 
				self->pixel_aspect_ratio_denom, NULL);
	}

	gst_structure_set (struc,
			"interlaced", G_TYPE_BOOLEAN, self->interlaced, NULL);


    gst_caps_append_structure (caps, struc);

    return caps;
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GOmxCore *gomx;
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE paramPort;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE numChannels;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
    OMX_CONFIG_ALG_ENABLE algEnable;
    GstOmxBaseVfpc *self;

    gomx = (GOmxCore *) omx_base->gomx;
    self = GST_OMX_BASE_VFPC (omx_base);

    GST_LOG_OBJECT (self, "begin");

    /* set the output cap */
    gst_pad_set_caps (omx_base->srcpad, create_src_caps (omx_base));
    
    /* Setting Memory type at input port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting input port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = self->input_port_index;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;    
    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    if (err != OMX_ErrorNone)
        return;

    /* Setting Memory type at output port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting output port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = self->output_port_index;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    if (err != OMX_ErrorNone)
        return;

    /* Input port configuration. */
    GST_LOG_OBJECT (self, "Setting port definition (input)");

    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &paramPort);
    paramPort.format.video.nFrameWidth = self->in_width;
    paramPort.format.video.nFrameHeight = self->in_height;
    paramPort.format.video.nStride = self->in_stride;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    paramPort.nBufferSize =  self->in_stride * self->in_height * 1.5;
    paramPort.nBufferAlignment = 0;
    paramPort.bBuffersContiguous = 0;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &paramPort);
    g_omx_port_setup (omx_base->in_port, &paramPort);

    /* Output port configuration. */
    GST_LOG_OBJECT (self, "Setting port definition (output)");

    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &paramPort);
    paramPort.format.video.nFrameWidth = self->out_width;
    paramPort.format.video.nFrameHeight = self->out_height;
    paramPort.format.video.nStride = self->out_stride;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    paramPort.nBufferSize =  self->out_stride * self->out_height;
    paramPort.nBufferCountActual = 8;
    paramPort.nBufferAlignment = 0;
    paramPort.bBuffersContiguous = 0;
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &paramPort);
    g_omx_port_setup (omx_base->out_port, &paramPort);

    /* Set number of channles */
    GST_LOG_OBJECT (self, "Setting number of channels");

    _G_OMX_INIT_PARAM (&numChannels);
    numChannels.nNumChannelsPerHandle = 1;    
    err = OMX_SetParameter (gomx->omx_handle, 
        (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &numChannels);

    if (err != OMX_ErrorNone)
        return;

    /* Set input channel resolution */
    GST_LOG_OBJECT (self, "Setting channel resolution (input)");

    _G_OMX_INIT_PARAM (&chResolution);
    chResolution.Frm0Width = self->in_width;
    chResolution.Frm0Height = self->in_height;
    chResolution.Frm0Pitch = self->in_stride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = 0;
    chResolution.FrmStartY = 0;
    chResolution.FrmCropWidth = 0;
    chResolution.FrmCropHeight = 0;
    chResolution.eDir = OMX_DirInput;
    chResolution.nChId = 0;
    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

    if (err != OMX_ErrorNone)
        return;

    /* Set output channel resolution */
    GST_LOG_OBJECT (self, "Setting channel resolution (output)");

    _G_OMX_INIT_PARAM (&chResolution);
    chResolution.Frm0Width = self->out_width;
    chResolution.Frm0Height = self->out_height;
    chResolution.Frm0Pitch = self->out_stride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = 0;
    chResolution.FrmStartY = 0;
    chResolution.FrmCropWidth = 0;
    chResolution.FrmCropHeight = 0;
    chResolution.eDir = OMX_DirOutput;
    chResolution.nChId = 0;
    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

    if (err != OMX_ErrorNone)
        return;

    _G_OMX_INIT_PARAM (&algEnable);
    algEnable.nPortIndex = 0;
    algEnable.nChId = 0;
    algEnable.bAlgBypass = OMX_FALSE;

    err = OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &algEnable);

    if (err != OMX_ErrorNone)
        return;
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
    GstOmxBaseVfpc *self;

    self = GST_OMX_BASE_VFPC (instance);

    self->omx_setup = omx_setup;
    g_object_set (self, "port-index", 0, NULL);
}

