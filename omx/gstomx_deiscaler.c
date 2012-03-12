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

#include "gstomx_deiscaler.h"
#include "gstomx.h"
#include "gstomx_buffertransport.h"

/* Define this for local framerate divisor implementation */
#define LOCAL_FRAMERATE_DIV_IMPLEMENTATION 1

GSTOMX_BOILERPLATE (GstOmxMDeiScaler, gst_omx_mdeiscaler, GstOmxBaseVfpc2, GST_OMX_BASE_VFPC2_TYPE);
GSTOMX_BOILERPLATE (GstOmxHDeiScaler, gst_omx_hdeiscaler, GstOmxBaseVfpc2, GST_OMX_BASE_VFPC2_TYPE);

static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE ("sink",
                GST_PAD_SINK,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (
                        "{NV12}", "[ 0, max ]"))
        );

static GstStaticPadTemplate src_template_yuv2 =
        GST_STATIC_PAD_TEMPLATE ("src_00",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED ( "{YUY2}", "[ 0, max ]" ))
        );

static GstStaticPadTemplate src_template_nv12 =
        GST_STATIC_PAD_TEMPLATE ("src_01",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (
                        "{NV12}", "[ 0, max ]"))
        );

#define YUV2_OUTPUT_IDX 0
#define NV12_OUTPUT_IDX 1

static const guint32 src_fourcc_list[NUM_OUTPUTS] = {
	GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
	GST_MAKE_FOURCC ('N', 'V', '1', '2')
};

static const OMX_COLOR_FORMATTYPE omx_color_format_list[NUM_OUTPUTS] = {
	OMX_COLOR_FormatYCbYCr,
	OMX_COLOR_FormatYUV420SemiPlanar
};

static const double buffersize_multiplier_list[NUM_OUTPUTS] = { 1, 1.5 };
static const int rowstride_multiplier_list[NUM_OUTPUTS] = { 2, 1 };

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL for OMX.TI.VPSSM3.VFPC.DEIMDUALOUT component";
        details.klass = "Filter";
        details.description = "Deinterlace and Scale video using VPSS Scaler module ";
        details.author = "Harinarayan Bhatta";

        gst_element_class_set_details (element_class, &details);
    }

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template_nv12));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template_yuv2));
}

static GstCaps*
create_src_caps (GstOmxBaseFilter2 *omx_base, int idx)
{
    GstCaps *caps;    
    GstOmxBaseVfpc2 *self;
    int width, height, rowstride;
    GstStructure *struc;

    self = GST_OMX_BASE_VFPC2 (omx_base);
    caps = gst_pad_peer_get_caps (omx_base->srcpad[idx]);

	width = self->in_width;
	height = self->in_height;
	rowstride = 0;

    if ((NULL != caps) && (!gst_caps_is_empty (caps)) && (0 != gst_caps_get_size (caps)))
    {
        GstStructure *s;

        s = gst_caps_get_structure (caps, 0);

        if (!(gst_structure_get_int (s, "width", &width) &&
            gst_structure_get_int (s, "height", &height))) {
            width = self->in_width;
            height = self->in_height;    
        } else 
			gst_structure_get_int (s, "rowstride", &rowstride);
    }
	/* Workaround: Make width multiple of 16, otherwise, scaler crashes */
	width = (width+15) & 0xFFFFFFF0;

	if (caps) gst_caps_unref (caps);

    caps = gst_caps_new_empty ();
	if (rowstride) {
		struc = gst_structure_new (("video/x-raw-yuv-strided"),
				"width",  G_TYPE_INT, width,
				"height", G_TYPE_INT, height,
				"rowstride", G_TYPE_INT, rowstride,
				"format", GST_TYPE_FOURCC, src_fourcc_list[idx],
				NULL);
	} else {
		struc = gst_structure_new (("video/x-raw-yuv"),
				"width",  G_TYPE_INT, width,
				"height", G_TYPE_INT, height,
				"format", GST_TYPE_FOURCC, src_fourcc_list[idx],
				NULL);
	}

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
			"interlaced", G_TYPE_BOOLEAN, FALSE, NULL);


    gst_caps_append_structure (caps, struc);

    return caps;
}

static void
omx_setup (GstOmxBaseFilter2 *omx_base)
{
    GOmxCore *gomx;
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE paramPort;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE numChannels;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
    OMX_CONFIG_ALG_ENABLE algEnable;
	OMX_CONFIG_SUBSAMPLING_FACTOR sSubSamplinginfo = {NULL};

    GstOmxBaseVfpc2 *self;
	int i, x, y, shift = 0;

    gomx = (GOmxCore *) omx_base->gomx;
    self = GST_OMX_BASE_VFPC2 (omx_base);

    GST_LOG_OBJECT (self, "begin");

	omx_base->input_fields_separately = self->interlaced;

	if (omx_base->duration != GST_CLOCK_TIME_NONE) {
		omx_base->duration *= (GST_OMX_DEISCALER(self))->framerate_divisor;
		self->framerate_denom *= (GST_OMX_DEISCALER(self))->framerate_divisor;
		if (omx_base->input_fields_separately) {
			// Halve the duration of output frame
			omx_base->duration = omx_base->duration/2;
			self->framerate_num *= 2;
		}
	}

    /* set the output cap */
	for (i=0 ; i<NUM_OUTPUTS; i++)
    	gst_pad_set_caps (omx_base->srcpad[i], create_src_caps (omx_base, i));
    
    /* Setting Memory type at input port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting input port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = omx_base->in_port->port_index;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;    
    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    if (err != OMX_ErrorNone)
        return;

    /* Setting Memory type at output port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting output port to Raw memory");

	for (i=0 ; i<NUM_OUTPUTS; i++) {
		_G_OMX_INIT_PARAM (&memTypeCfg);
		memTypeCfg.nPortIndex = omx_base->out_port[i]->port_index;
		memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
		err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

		if (err != OMX_ErrorNone)
			return;
	}

	if (self->interlaced) shift = 1;
    /* Input port configuration. */
    GST_LOG_OBJECT (self, "Setting port definition (input)");

    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &paramPort);
    paramPort.format.video.nFrameWidth = self->in_width;
    paramPort.format.video.nFrameHeight = self->in_height >> shift;
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

	for (i=0 ; i<NUM_OUTPUTS; i++) {
		G_OMX_PORT_GET_DEFINITION (omx_base->out_port[i], &paramPort);
		paramPort.format.video.nFrameWidth = self->out_width[i];
		paramPort.format.video.nFrameHeight = self->out_height[i];
		paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
		paramPort.format.video.nStride = self->out_stride[i];
		paramPort.format.video.eColorFormat = omx_color_format_list[i];
		paramPort.nBufferSize =  self->out_stride[i] * self->out_height[i] * buffersize_multiplier_list[i];
		paramPort.nBufferCountActual = 8;
		paramPort.nBufferAlignment = 0;
		paramPort.bBuffersContiguous = 0;
		G_OMX_PORT_SET_DEFINITION (omx_base->out_port[i], &paramPort);
		g_omx_port_setup (omx_base->out_port[i], &paramPort);
	}

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
    chResolution.Frm0Height = self->in_height >> shift;
    chResolution.Frm0Pitch = self->in_stride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = x;
    chResolution.FrmStartY = y;
    chResolution.FrmCropWidth = self->in_width;
    chResolution.FrmCropHeight = self->in_height >> shift;
    chResolution.eDir = OMX_DirInput;
    chResolution.nChId = 0;
    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

    if (err != OMX_ErrorNone)
        return;

	_G_OMX_INIT_PARAM(&sSubSamplinginfo);
#ifdef LOCAL_FRAMERATE_DIV_IMPLEMENTATION
	sSubSamplinginfo.nSubSamplingFactor = 1;
#else
	sSubSamplinginfo.nSubSamplingFactor = (GST_OMX_DEISCALER(self))->framerate_divisor;
#endif
	err = OMX_SetConfig ( gomx->omx_handle, ( OMX_INDEXTYPE )
			( OMX_TI_IndexConfigSubSamplingFactor ),
			&sSubSamplinginfo );
	if (err != OMX_ErrorNone)
		return;

    /* Set output channel resolution */
    GST_LOG_OBJECT (self, "Setting channel resolution (output)");

    _G_OMX_INIT_PARAM (&chResolution);
    chResolution.Frm0Width = self->out_width[0];
    chResolution.Frm0Height = self->out_height[0];
    chResolution.Frm0Pitch = self->out_stride[0];
    chResolution.Frm1Width = self->out_width[1];
    chResolution.Frm1Height = self->out_height[1];
    chResolution.Frm1Pitch = self->out_stride[1];
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
    algEnable.bAlgBypass = (self->interlaced)?OMX_FALSE:OMX_TRUE;

    err = OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &algEnable);

    if (err != OMX_ErrorNone)
        return;
}

enum
{
    ARG_0,
    ARG_FRAMERATE_DIV,
};

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    switch (prop_id)
    {
        case ARG_FRAMERATE_DIV:
            (GST_OMX_DEISCALER(obj))->framerate_divisor = g_value_get_uint (value);
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
    switch (prop_id)
    {
        case ARG_FRAMERATE_DIV:
            g_value_set_uint (value, (GST_OMX_DEISCALER(obj))->framerate_divisor);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (g_class);
	gobject_class->set_property = set_property;
	gobject_class->get_property = get_property;

	g_object_class_install_property (gobject_class, ARG_FRAMERATE_DIV,
			g_param_spec_uint ("framerate-divisor", "Output framerate divisor",
				"Output framerate = (2 * input_framerate) / framerate_divisor",
				1, 60, 1, G_PARAM_READWRITE));
}

static gboolean push_cb (GstOmxBaseFilter2 *self, GstBuffer *buf)
{
	int i;
	gboolean ret;
	for (i = 0; i< NUM_OUTPUTS; i++) {
		if (GST_GET_OMXPORT(buf) == self->out_port[i]) {
			break;
		}
	}
	ret = ((GST_OMX_DEISCALER(self))->framecnt[i] == 0);
	(GST_OMX_DEISCALER(self))->framecnt[i]++;
	if ((GST_OMX_DEISCALER(self))->framecnt[i] ==
	    (GST_OMX_DEISCALER(self))->framerate_divisor)
		(GST_OMX_DEISCALER(self))->framecnt[i] = 0;
	return ret;
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseVfpc2 *self;
	GstOmxBaseFilter2 *base_filter;
	int i;

    self = GST_OMX_BASE_VFPC2 (instance);
    base_filter = GST_OMX_BASE_FILTER2 (instance);

    self->omx_setup = omx_setup;
#ifdef LOCAL_FRAMERATE_DIV_IMPLEMENTATION
	base_filter->push_cb = push_cb;
#endif

	(GST_OMX_DEISCALER(instance))->framerate_divisor = 1;
	for (i=0; i<NUM_OUTPUTS; i++) (GST_OMX_DEISCALER(instance))->framecnt[i] = 0;
}

