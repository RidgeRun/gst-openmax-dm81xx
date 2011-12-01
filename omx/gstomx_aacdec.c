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

#include "gstomx_aacdec.h"
#include "gstomx.h"

#include <string.h> /* for memset */
#ifdef USE_OMXTIAUDIODEC
#  include <audio_decode/TIDspOmx.h>
#endif

enum
{
	ARG_0,
	ARG_FRAMEMODE,
};

#define FRAMEMODE_DEFAULT FALSE

GSTOMX_BOILERPLATE (GstOmxAacDec, gst_omx_aacdec, GstOmxBaseAudioDec, GST_OMX_BASE_AUDIODEC_TYPE);

typedef enum
{
	AAC_PROFILE_LC = 2,
	AAC_PROFILE_LC_SBR = 5,
	AAC_PROFILE_LC_SBR_PS = 6,
} AacVersion;

	static GstCaps *
generate_src_template (void)
{
	GstCaps *caps;

	caps = gst_caps_new_simple ("audio/x-raw-int",
			"endianness", G_TYPE_INT, G_BYTE_ORDER,
			"width", G_TYPE_INT, 16,
			"depth", G_TYPE_INT, 16,
			"rate", GST_TYPE_INT_RANGE, 8000, 96000,
			"signed", G_TYPE_BOOLEAN, TRUE,
			"channels", GST_TYPE_INT_RANGE, 1, 8,
			NULL);

	return caps;
}

	static GstCaps *
generate_sink_template (void)
{
	GstCaps *caps;
	GstStructure *struc;
	caps = gst_caps_new_empty ();

	struc = gst_structure_new ("audio/mpeg",
			"mpegversion", G_TYPE_INT, 4,
			"channels", GST_TYPE_INT_RANGE, 1, 8,
			"rate", GST_TYPE_INT_RANGE, 8000, 96000,
			"object_type", GST_TYPE_INT_RANGE, 1, 6,
			"parsed", G_TYPE_BOOLEAN, TRUE,
			NULL);

	{
		GValue list;
		GValue val;

		list.g_type = val.g_type = 0;

		g_value_init (&list, GST_TYPE_LIST);
		g_value_init (&val, G_TYPE_INT);

		g_value_set_int (&val, 2);
		gst_value_list_append_value (&list, &val);

		g_value_set_int (&val, 4);
		gst_value_list_append_value (&list, &val);

		gst_structure_set_value (struc, "mpegversion", &list);

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

		details.longname = "OpenMAX IL AAC audio decoder";
		details.klass = "Codec/Decoder/Audio";
		details.description = "Decodes audio in AAC format with OpenMAX IL";
		details.author = "Felipe Contreras";

		gst_element_class_set_details (element_class, &details);
	}

	{
		GstPadTemplate *template;

		template = gst_pad_template_new ("src", GST_PAD_SRC,
				GST_PAD_ALWAYS,
				generate_src_template ());

		gst_element_class_add_pad_template (element_class, template);
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
set_property (GObject *obj,
		guint prop_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GstOmxAacDec *self;

	self = GST_OMX_AACDEC (obj);

	switch (prop_id)
	{
		case ARG_FRAMEMODE:
			self->framemode = g_value_get_boolean  (value);
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
	GstOmxAacDec *self;

	self = GST_OMX_AACDEC (obj);

	switch (prop_id)
	{
		case ARG_FRAMEMODE:
			g_value_set_boolean (value, self->framemode);
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

	/* Properties stuff */
	{
		gobject_class->set_property = set_property;
		gobject_class->get_property = get_property;

		g_object_class_install_property (gobject_class, ARG_FRAMEMODE,
				g_param_spec_boolean ("framemode", "Frame Mode",
					"Frame Mode", FRAMEMODE_DEFAULT, G_PARAM_READWRITE));
	}
}

	static gboolean
sink_setcaps (GstPad *pad,
		GstCaps *caps)
{
	GstStructure *structure;
	GstOmxBaseFilter *omx_base;
	GstOmxBaseAudioDec *base_audiodec;
	GstOmxAacDec* self;


	omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
	base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);
	self = GST_OMX_AACDEC (omx_base);

	GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

	structure = gst_caps_get_structure (caps, 0);


	base_audiodec->rate = 44100;
	gst_structure_get_int (structure, "rate", &base_audiodec->rate);

	base_audiodec->channels = 2;
	gst_structure_get_int (structure, "channels", &base_audiodec->channels);

	self->aacversion = 2;
	gst_structure_get_int (structure, "object_type", &self->aacversion);

	self->framed = gst_structure_has_field (structure, "framed");

	omx_base->in_port->caps = gst_caps_copy (caps);



	return gst_pad_set_caps (pad, caps);
}
	static gboolean
src_setcaps (GstPad *pad,
		GstCaps *caps)
{
	GstStructure *structure;
	GstOmxBaseFilter *omx_base;
	GstOmxBaseAudioDec *base_audiodec;
	GstOmxAacDec* self;


	omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
	base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);
	self = GST_OMX_AACDEC (omx_base);

	GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

	structure = gst_caps_get_structure (caps, 0);


	base_audiodec->rate = 44100;
	//gst_structure_get_int (structure, "rate", &base_audiodec->rate);

	base_audiodec->channels = 2;
	//gst_structure_get_int (structure, "channels", &base_audiodec->channels);

	self->aacversion = 2;
	//gst_structure_get_int (structure, "object_type", &self->aacversion);

	self->framed = gst_structure_has_field (structure, "framed");

	omx_base->out_port->caps = gst_caps_copy (caps);


	return gst_pad_set_caps (pad, caps);
}

	static GstCaps *
src_getcaps (GstPad *pad)
{
	GstCaps *caps;
	GstStructure *structure;
	GstOmxBaseFilter *omx_base;
	GstOmxBaseAudioDec *base_audiodec;
	GstOmxAacDec* self;


	omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
	base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);
	self = GST_OMX_AACDEC (omx_base);
	{ 
		/* set pcm port */
		OMX_AUDIO_PARAM_PCMMODETYPE param;

	}

	return caps;
}
	static GstCaps *
sink_getcaps (GstPad *pad)
{
	GstCaps *caps = NULL;
	GstStructure *structure = NULL;
	GstOmxBaseFilter *omx_base;
	GstOmxBaseAudioDec *base_audiodec;
	GstOmxAacDec* self;


	omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
	base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);
	self = GST_OMX_AACDEC (omx_base);


	//OMX_AUDIO_PARAM_AACPROFILETYPE param;

	return caps;
}

	static void
omx_setup (GstOmxBaseFilter *omx_base)
{
	GstOmxBaseAudioDec *base_audiodec = GST_OMX_BASE_AUDIODEC (omx_base);
	GstOmxAacDec *self = GST_OMX_AACDEC (omx_base);
	//OMX_ERRORTYPE eError = OMX_ErrorUndefined;
	OMX_PORT_PARAM_TYPE portInit;
	OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
	GOmxPort *port;
	GOmxCore *gomx;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	OMX_U32 streamFormat;
	gint profile;

	gomx = (GOmxCore *) omx_base->gomx;

	GST_DEBUG_OBJECT (omx_base, "Begin Set-Up");

	switch (self->aacversion)
	{
		case AAC_PROFILE_LC_SBR_PS:
			profile = OMX_AUDIO_AACObjectHE_PS;
			break;
		case AAC_PROFILE_LC_SBR:
			profile = OMX_AUDIO_AACObjectHE;
			break;
		case AAC_PROFILE_LC:
		default:
			profile = OMX_AUDIO_AACObjectLC;
			break;
	}

	// Does it come from a demuxer?
	if(self->framed)
	{
		streamFormat = OMX_AUDIO_AACStreamFormatRAW;
		GST_DEBUG_OBJECT (omx_base, "Format: Raw");
	}
	else
	{
		streamFormat = OMX_AUDIO_AACStreamFormatMax;
		GST_DEBUG_OBJECT (omx_base, "Format: Max");
	}

	{
		OMX_AUDIO_PARAM_AACPROFILETYPE param;
		G_OMX_PORT_GET_PARAM (omx_base->in_port, OMX_IndexParamAudioAac, &param);
		param.eAACProfile = profile;
		param.eAACStreamFormat = streamFormat;
		G_OMX_PORT_SET_PARAM (omx_base->in_port, OMX_IndexParamAudioAac, &param);
	}

	{
		OMX_AUDIO_PARAM_PCMMODETYPE param;
		G_OMX_PORT_GET_PARAM (omx_base->out_port, OMX_IndexParamAudioPcm, &param);
		param.nSamplingRate = base_audiodec->rate;
		GST_DEBUG_OBJECT (omx_base, "PCM Sample Rate: %ld", param.nSamplingRate);
		G_OMX_PORT_SET_PARAM (omx_base->out_port, OMX_IndexParamAudioPcm, &param);
	}
	/*set port definition*/
#if 1
	_G_OMX_INIT_PARAM(&portInit);
	portInit.nPorts = 2;
	portInit.nStartPortNumber = 0;
	G_OMX_PORT_SET_PARAM(omx_base->in_port,OMX_IndexParamAudioInit,&portInit);

	_G_OMX_INIT_PARAM(&pInPortDef);
	pInPortDef.nPortIndex = 0;//OMX_AUDDEC_INPUT_PORT;
	pInPortDef.eDir = OMX_DirInput;
	pInPortDef.nBufferCountActual = 1;
	pInPortDef.nBufferCountMin = 1;
	pInPortDef.nBufferSize = 4096; //width * height;

	pInPortDef.bEnabled = OMX_TRUE;
	pInPortDef.bPopulated = OMX_FALSE;
	pInPortDef.eDomain = OMX_PortDomainAudio;
	pInPortDef.bBuffersContiguous = OMX_FALSE;
	pInPortDef.nBufferAlignment = 32;

	/* OMX_VIDEO_PORTDEFINITION values for input port */
	pInPortDef.format.audio.cMIMEType = "ADEC";
	pInPortDef.format.audio.pNativeRender = NULL;
	pInPortDef.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
	pInPortDef.format.audio.bFlagErrorConcealment = OMX_FALSE;
	G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &pInPortDef);

	_G_OMX_INIT_PARAM(&pOutPortDef);
	pOutPortDef.nPortIndex = 1;
	pOutPortDef.eDir = OMX_DirOutput;
	pOutPortDef.nBufferCountActual = 1;
	pOutPortDef.nBufferCountMin = 1;

	/* for referance purpose, in PT mode, stride and height would be used by FQ
	   It is padded inside components */
	pOutPortDef.nBufferSize = 4608;        
	pOutPortDef.bEnabled = OMX_TRUE;
	pOutPortDef.bPopulated = OMX_FALSE;
	pOutPortDef.eDomain = OMX_PortDomainAudio;
	pOutPortDef.bBuffersContiguous = OMX_FALSE;
	pOutPortDef.nBufferAlignment = 32;

	/* OMX_VIDEO_PORTDEFINITION values for output port */
	pOutPortDef.format.audio.cMIMEType = "PCM";
	pOutPortDef.format.audio.pNativeRender = NULL;
	pOutPortDef.format.audio.bFlagErrorConcealment = OMX_FALSE;
	pOutPortDef.format.audio.eEncoding = OMX_AUDIO_CodingUnused;
	G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &pOutPortDef);
#endif
	port = g_omx_core_get_port (gomx, "input", 0);
	GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %x)", port->port_index);
	eError = OMX_SendCommand (g_omx_core_get_handle (port->core),
			OMX_CommandPortEnable, port->port_index, NULL);
	g_sem_down (port->core->port_sem);
	if (eError != OMX_ErrorNone) {
		printf("\nSend command for port enable on port index=%d failed returned eError=%x",eError);
	}

	port = g_omx_core_get_port (gomx, "output", 1);
	GST_DEBUG_OBJECT(self, "SendCommand(PortEnable, %x)", port->port_index);
	eError = OMX_SendCommand (g_omx_core_get_handle (port->core),
			OMX_CommandPortEnable, 1, NULL);
	g_sem_down (port->core->port_sem);
	if (eError != OMX_ErrorNone) {
		printf("\nSend command for port enable on port index=%d failed returned eError=%x",port->port_index,eError);
	}

#ifdef USE_OMXTIAUDIODEC
	// This is specific for TI.
	{
		OMX_INDEXTYPE index;
		TI_OMX_DSP_DEFINITION audioinfo;

		GOmxCore *gomx = omx_base->gomx;

		memset (&audioinfo, 0, sizeof (audioinfo));

		audioinfo.framemode = self->framemode;
		GST_DEBUG_OBJECT (omx_base, "Frame Mode: %d", audioinfo.framemode);

		g_assert(
				OMX_GetExtensionIndex (
					gomx->omx_handle, "OMX.TI.index.config.aacdecHeaderInfo",
					&index) == OMX_ErrorNone);

		g_assert(
				OMX_SetConfig (
					gomx->omx_handle, index,
					&audioinfo) == OMX_ErrorNone);

		GST_DEBUG_OBJECT (omx_base, "End Set-Up");
	}
#endif
}

	static void
type_instance_init (GTypeInstance *instance,
		gpointer g_class)
{
	GstOmxBaseFilter *omx_base;
	GstOmxAacDec *self;

	self = GST_OMX_AACDEC (instance);
	omx_base = GST_OMX_BASE_FILTER (instance);
	//GST_DEBUG_OBJECT (omx_base, "start");
	omx_base->omx_setup = omx_setup;
	//omx_base->in_port->always_copy  = FALSE;
	omx_base->out_port->always_copy = TRUE;

	gst_pad_set_setcaps_function (omx_base->sinkpad, sink_setcaps);
	gst_pad_set_getcaps_function (omx_base->srcpad, src_getcaps);
	gst_pad_set_getcaps_function (omx_base->sinkpad, sink_getcaps);
	gst_pad_set_setcaps_function (omx_base->srcpad, src_setcaps);
}


