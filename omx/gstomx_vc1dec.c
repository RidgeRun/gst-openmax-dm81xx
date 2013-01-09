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

#include "gstomx_vc1dec.h"
#include "gstomx.h"

#define     PADX    16
#define     PADY    16
#define UTIL_ALIGN(a,b)  ((((OMX_U32)(a)) + (b)-1) & (~((OMX_U32)((b)-1))))

GSTOMX_BOILERPLATE (GstOmxVC1Dec, gst_omx_vc1dec, GstOmxBaseVideoDec, GST_OMX_BASE_VIDEODEC_TYPE);

#if 0
static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

    struc = gst_structure_new ("video/x-wmv",
                               "wmvversion", G_TYPE_INT, 3,
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    gst_caps_append_structure (caps, struc);

    return caps;
}
#endif

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-wmv, "
			"wmvversion=(int)3, "
			"parsed=(boolean)true,"
			"height=(int) [ 16, 4096], "
			"width=(int) [ 16, 4096], "
			"framerate=(fraction) [ 0, MAX], "
			"wmvprofile=(string){ simple, main, advanced}")
);


#if 0 // SRC pad template already exists in gstomx_base_videodec.c
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-raw-yuv, "                        /* UYVY */
         "format=(fourcc)UYVY, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
);
#endif

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL vc1/WMV video decoder";
        details.klass = "Codec/Decoder/Video";
        details.description = "Decodes video in vc1/WMV format with OpenMAX IL";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    {
#if 0
        GstPadTemplate *template;

        template = gst_pad_template_new ("sink", GST_PAD_SINK,
                                         GST_PAD_ALWAYS,
                                         generate_sink_template ());

        gst_element_class_add_pad_template (element_class, template);

        gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
#endif
        gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    }
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *capStruct;
    GstOmxBaseVideoDec *self;
    GstOmxBaseFilter *omx_base;
    GOmxCore *gomx;
    GstBuffer *wmv_header_buf;
    VC1SequenceHdr vc1header;
    char *tmp;
    guint32 fourcc;

    gint width = 0;
    gint height = 0;

    self = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);
 
    capStruct = gst_caps_get_structure(caps, 0);
   
    if (!gst_structure_get_fourcc(capStruct, "format", &fourcc)) {
            fourcc = GST_MAKE_FOURCC('W','M','V','3'); 
    }
    
    if(fourcc == GST_MAKE_FOURCC('W','V','C','1')){
       GST_DEBUG_OBJECT (self, "Got wvc1 fourcc advance profile file");
       wmv_header_buf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE (omx_base->codec_data)+3);
       if (wmv_header_buf == NULL) {
          GST_ERROR("Failed to allocate memory for wmv advance profile header\n");
	  return NULL;
       }
       else{
          GST_DEBUG_OBJECT (self, "Allocated buffer for wmv header of size =%d",GST_BUFFER_SIZE (omx_base->codec_data)+3);
       }
   
       if (omx_base->codec_data){
          tmp = (char*)GST_BUFFER_DATA(omx_base->codec_data);
          //memcpy(GST_BUFFER_DATA(wmv_header_buf),(tmp+1),21);
          memcpy(GST_BUFFER_DATA(wmv_header_buf),(tmp+1),(GST_BUFFER_SIZE (omx_base->codec_data)-1));
	  tmp = (char*)GST_BUFFER_DATA(wmv_header_buf);
	  tmp[GST_BUFFER_SIZE (omx_base->codec_data)-1] = 0;
	  tmp[GST_BUFFER_SIZE (omx_base->codec_data)] = 0;
	  tmp[GST_BUFFER_SIZE (omx_base->codec_data)+1] = 1;
	  tmp[GST_BUFFER_SIZE (omx_base->codec_data)+2] = 0xd;
	  GST_BUFFER_SIZE(wmv_header_buf) = GST_BUFFER_SIZE (omx_base->codec_data)+3;
          omx_base->codec_data = wmv_header_buf;
          gst_buffer_ref (wmv_header_buf);
          
       }
       else{
          GST_ERROR("No codec data");
       }
    }//if(fourcc == GST_MAKE_FOURCC('W','V','C','1'))
    else{
       GST_DEBUG_OBJECT (self, "got simple profile file");
       wmv_header_buf = gst_buffer_new_and_alloc(sizeof(VC1SequenceHdr));
       if (wmv_header_buf == NULL) {
          GST_ERROR("Failed to allocate memory for wmv advance profile header\n");
	  return NULL;
       }
       else{
          GST_DEBUG_OBJECT (self, "Allocated buffer for wmv header of size =%d",GST_BUFFER_SIZE (omx_base->codec_data)+3);
       }
       if (omx_base->codec_data){
          //omx_base->pVC1SqHdr = (struct VC1SequenceHdr*)malloc(sizeof(struct VC1SequenceHdr));
          vc1header.nFrames = (0xc5 << 24)| (100);
	  vc1header.resv1 = 0x4;
          tmp = (char*)GST_BUFFER_DATA(omx_base->codec_data);
	  memcpy(&vc1header.StructC,tmp,4); // 4 bytes of sequence header
	  vc1header.StructA.width 	= self->extendedParams.height;
	  vc1header.StructA.height  = self->extendedParams.width;
	  vc1header.resv2 = 0x0000000C;
	  vc1header.StructB[0] = 0;
	  vc1header.StructB[1] = 0;
	  vc1header.StructB[2] = 0;
          memcpy(GST_BUFFER_DATA(wmv_header_buf),&vc1header,36);
          GST_BUFFER_SIZE(wmv_header_buf) = 36;
          omx_base->codec_data = wmv_header_buf;
       }
       else{
        GST_ERROR("No codec data");
       }

    }
    return gst_pad_set_caps (pad, caps);
    
}


static void
initialize_port (GstOmxBaseFilter *omx_base)
{
    
    GstOmxBaseVideoDec *self;
    GOmxCore *gomx;
    OMX_PORT_PARAM_TYPE portInit;
    OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
    gint width, height;
    GOmxPort *port;

    self = GST_OMX_BASE_VIDEODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    width = self->extendedParams.width;
    height = self->extendedParams.height;
    _G_OMX_INIT_PARAM(&portInit);
    portInit.nPorts = 2;
    portInit.nStartPortNumber = 0;
    G_OMX_PORT_SET_PARAM(omx_base->in_port,OMX_IndexParamAudioInit,&portInit);

    _G_OMX_INIT_PARAM(&pInPortDef);
    
    /* populate the input port definataion structure, It is Standard OpenMax
      structure */
    /* set the port index */
    pInPortDef.nPortIndex = 0;//OMX_VIDDEC_INPUT_PORT;
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
	
    pInPortDef.format.video.cMIMEType = "VC1";
    pOutPortDef.format.video.cMIMEType = "VC1";
    pOutPortDef.nBufferSize =
	((((width + (2 * 16) + 127) & 0xFFFFFF80) * ((height +
				      (4 * 16))) * 3) >> 1) +
	256;
    pInPortDef.format.video.pNativeRender = NULL;
    /* set the width and height, used for buffer size calculation */
    pInPortDef.format.video.nFrameWidth = width;
    pInPortDef.format.video.nFrameHeight = height;
    /* for bitstream buffer stride is not a valid parameter */
    pInPortDef.format.video.nStride = -1;
    /* component supports only frame based processing */
    pInPortDef.format.video.nSliceHeight = 0;

    /* bitrate does not matter for decoder */
    //pInPortDef.format.video.nBitrate = 104857600;
    /* as per openmax frame rate is in Q16 format */
    //pInPortDef.format.video.xFramerate = 60 << 16;
    pInPortDef.format.video.xFramerate = 60<<16;//(self->framerate_num/self->framerate_denom)<<16;
    /* input port would receive H264 stream */
    pInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;//compressionFormat;
    /* this is codec setting, OMX component does not support it */
    pInPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
    /* color format is irrelavant */
    pInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &pInPortDef);

    _G_OMX_INIT_PARAM(&pOutPortDef);
    pOutPortDef.nPortIndex = 1;//OMX_VIDDEC_OUTPUT_PORT;
    pOutPortDef.eDir = OMX_DirOutput;
    /* componet would expect these numbers of buffers to be allocated */
    pOutPortDef.nBufferCountActual = 8;
    pOutPortDef.nBufferCountMin = 1;

    /* Codec requires padded height and width and width needs to be aligned at
    128 byte boundary */
	
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
    pOutPortDef.format.video.nStride = UTIL_ALIGN (width + (2 * PADX), 128);
    pOutPortDef.format.video.nSliceHeight = 0;
    /* bitrate does not matter for decoder */
    //pOutPortDef.format.video.nBitrate = 25000000;
    /* as per openmax frame rate is in Q16 format */
    //pOutPortDef.format.video.xFramerate = 60 << 16;
    pOutPortDef.format.video.xFramerate = 60<<16;//(self->framerate_num/self->framerate_denom)<<16;
    pOutPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
    /* output is raw YUV 420 SP format, It support only this */
    pOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &pOutPortDef);


    port = g_omx_core_get_port (gomx, "input", 0);
    port = g_omx_core_get_port (gomx, "output", 1);
    GST_INFO_OBJECT (omx_base, "end");
    
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    
    GstOmxBaseVideoDec *omx_base;
    omx_base = GST_OMX_BASE_VIDEODEC (instance);
    omx_base->compression_format = OMX_VIDEO_CodingWMV;
    omx_base->initialize_port = initialize_port;
    omx_base->sink_setcaps = sink_setcaps;
    
}
