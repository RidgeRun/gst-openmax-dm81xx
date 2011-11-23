/*
 * Created on: Aug 17, 2009
 *
 * This is the first version for the jpeg decoder on gst-openmax
 *
 * Copyright (C) 2009 Texas Instruments - http://www.ti.com/
 *
 * Author:
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "gstomx_jpegdec.h"
#include "gstomx_base_filter.h"
#include "gstomx.h"

#include <string.h>
#include <stdlib.h>

/*these should change in the future to 864 x 480 (LCD resolution)*/
#define MAX_WIDTH 176
#define MAX_HEIGHT 144

GSTOMX_BOILERPLATE (GstOmxJpegDec, gst_omx_jpegdec, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);


static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_YUV_STRIDED ( "{ NV12, UYVY }", "[ 0, max ]") ";")
);

static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE ("sink",
                GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS ("image/jpeg, "
                        "width = (int)[16,MAX], "
                        "height = (int)[16,MAX], "
                        "framerate = (fraction)[0/1,max];")
        );

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL JPEG image decoder";
        details.klass = "Codec/Decoder/Image";
        details.description = "Decodes image in JPEG format with OpenMAX IL";
        details.author = "Texas Instrument";

        gst_element_class_set_details (element_class, &details);
    }

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&src_template));

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&sink_template));
}

/*The properties have not been implemented yet*/
/***
static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxJpegDec *self;

    self = GST_OMX_JPEGDEC (obj);

    switch (prop_id)
    {
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
    GstOmxJpegDec *self;

    self = GST_OMX_JPEGDEC (obj);

    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}
**/
static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (g_class);

    /* Properties stuff */
    /*{
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;
    }
    */
}
static GstCaps *
fixcaps (GstCaps* mycaps, GstCaps* intercaps)
{
    GstStructure *ins, *outs;
    gint my_w, my_h, n_h, n_w;
    gint p_w = 0, p_h = 0, i=0;

    /*From preloaded caps we are only taken the fist structure to know width an d height*/
    ins = gst_caps_get_structure (mycaps, 0);
    outs = gst_caps_get_structure (intercaps, 0);

    gst_structure_get_int (outs, "width", &p_w);
    gst_structure_get_int (outs, "height", &p_h);
    gst_structure_get_int (ins, "width", &my_w);
    gst_structure_get_int (ins, "height", &my_h);

    if ( p_h && ( my_h >= MAX_HEIGHT) && ( p_h < my_h) )
    {
        n_h = GST_ROUND_UP_2 (p_h);
    }
    else
    {
        n_h = my_h;
        GST_WARNING ( "Height value is missing or resize is not supported for this image size");
    }
    if (p_w && ( my_w >= MAX_WIDTH) && (p_w < my_w) )
    {
        n_w = GST_ROUND_UP_2 (p_w);
    }
    else
    {
        n_w = my_w;
        GST_WARNING (" Width value is missing resize is not supported for this image size");
    }
    /* now fixate */
    intercaps = gst_caps_make_writable (intercaps);

    for (i=0; i<gst_caps_get_size (intercaps); i++)
    {
        GstStructure *struc = gst_caps_get_structure (intercaps, i);
        gst_structure_set (struc, "width", G_TYPE_INT, n_w, NULL);
        gst_structure_set (struc, "height", G_TYPE_INT, n_h, NULL);
    }

    return intercaps;
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseFilter *omx_base;
    GstOmxJpegDec *self;
    GstCaps *inter_caps, *fixed_caps, *new_caps;

    omx_base = core->object;
    self = GST_OMX_JPEGDEC (omx_base);

    GST_DEBUG_OBJECT (omx_base, "settings changed cb ");

    inter_caps = gst_caps_intersect ( gst_static_pad_template_get_caps (&src_template),
                                gst_pad_peer_get_caps (omx_base->srcpad));

    fixed_caps = fixcaps ( gst_pad_get_caps (omx_base->srcpad), inter_caps);

    new_caps = gst_caps_intersect ( fixed_caps, gst_pad_peer_get_caps (omx_base->srcpad)) ;

    if (!gst_caps_is_fixed (new_caps))
    {
        gst_caps_do_simplify (new_caps);
        gst_pad_fixate_caps (omx_base->srcpad, new_caps);
        GST_INFO_OBJECT (omx_base, "pre-fixated caps: %" GST_PTR_FORMAT, new_caps);
    }

    gst_pad_set_caps (omx_base->srcpad, new_caps);
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{

    GstStructure *structure;
    GstOmxBaseFilter *omx_base;
    GstOmxJpegDec *self;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE param;
    gint width = 0;
    gint height = 0;
    OMX_COLOR_FORMATTYPE color_format = OMX_COLOR_FormatCbYCrY;

    omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    self = GST_OMX_JPEGDEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

    structure = gst_caps_get_structure (caps, 0);

    gst_structure_get_int (structure, "width", &width);
    gst_structure_get_int (structure, "height", &height);
    gst_structure_get_boolean (structure, "interlaced", &self->progressive);

    height = GST_ROUND_UP_16 ( height );
    width = GST_ROUND_UP_16 ( width );

    {
        guint32 fourcc;
        if (gst_structure_get_fourcc (structure, "format", &fourcc))
        {
            color_format = g_omx_fourcc_to_colorformat (fourcc);
        }
    }

    {
        const GValue *framerate = NULL;
        framerate = gst_structure_get_value (structure, "framerate");
        if (framerate)
        {
            self->framerate_num = gst_value_get_fraction_numerator (framerate);
            self->framerate_denom = gst_value_get_fraction_denominator (framerate);
            if (self->framerate_num == 0)
            {
                omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,0,1);
            }
            else
            {
                omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,
                        gst_value_get_fraction_denominator (framerate),
                        gst_value_get_fraction_numerator (framerate));
            }

            GST_DEBUG_OBJECT (self, "Nominal frame duration =%"GST_TIME_FORMAT,
                                    GST_TIME_ARGS (omx_base->duration));
        }
    }

    {
        const GValue *codec_data;
        GstBuffer *buffer;

        codec_data = gst_structure_get_value (structure, "codec_data");
        if (codec_data)
        {
            buffer = gst_value_get_buffer (codec_data);
            omx_base->codec_data = buffer;
            gst_buffer_ref (buffer);
        }
    }

    /* Input port configuration. */
    {
        G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

        param.format.image.nFrameWidth = width;
        param.format.image.nFrameHeight = height;
        param.format.image.eColorFormat = color_format;

        G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
    }

    return gst_pad_set_caps (pad, caps);

}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxJpegDec *self;
    GOmxCore *gomx;
    GstStructure* structure;

    GstOmxBaseFilter *omx_base;
    GstVideoFormat format;
    gint width, height, rowstride;
    OMX_PARAM_PORTDEFINITIONTYPE param;

    omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    self = GST_OMX_JPEGDEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    GST_INFO_OBJECT (omx_base, "begin set src caps");

    structure = gst_caps_get_structure (caps, 0);

    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

    if (gst_video_format_parse_caps_strided (caps,
                &format, &width, &height, &rowstride))
    {
        param.format.image.eColorFormat = g_omx_gstvformat_to_colorformat (format);
        param.nBufferSize = gst_video_format_get_size_strided (format, width, height, rowstride);

        param.format.image.nStride      = rowstride;
        param.format.image.nFrameWidth  = GST_ROUND_UP_2  (width);          /*Should be factor of 2 or 16 ?*/
        param.format.image.nFrameHeight = GST_ROUND_UP_2  (height);

        G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &param);

            GST_INFO_OBJECT (omx_base, "exit set src caps");

        return TRUE;
    }
    else
    {
            GST_WARNING_OBJECT (self, " GST_VIDEO_FORMAT_UNKNOWN ");
            return FALSE;
    }
}

static GstCaps *
src_getcaps (GstPad *pad)
{
    GstOmxBaseFilter *omx_base;
    GstOmxJpegDec *self;
    GstCaps  *poss_caps;
    omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    self = GST_OMX_JPEGDEC (omx_base);

    if (self->outport_configured)
    {
        OMX_PARAM_PORTDEFINITIONTYPE param;
        int i;

        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);
        poss_caps = gst_caps_new_empty ();

        /* note: we only support strided caps if outport buffer is shared:
         */
        for (i=0; i<(omx_base->out_port->share_buffer ? 2 : 1); i++)
        {
            GstStructure *struc = gst_structure_new (
                    (i ? "video/x-raw-yuv-strided" : "video/x-raw-yuv"),
                    "width",  G_TYPE_INT, param.format.image.nFrameWidth,
                    "height", G_TYPE_INT, param.format.image.nFrameHeight,
                    NULL);
            if(i)
            {
                gst_structure_set (struc, "rowstride", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
            }
            if (self->framerate_denom)
            {
                gst_structure_set (struc, "framerate", GST_TYPE_FRACTION, self->framerate_num,
                    self->framerate_denom, NULL);
            }
            gst_caps_append_structure (poss_caps, struc);
        }

        poss_caps = g_omx_port_set_image_formats (omx_base->out_port, poss_caps);

    }
    else
    {
        /* we don't have valid width/height/etc yet, so just use the template.. */
        poss_caps = gst_static_pad_template_get_caps (&src_template);
    }
    return poss_caps;
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxJpegDec *self;
    GOmxCore *gomx;
    gint width, height;
    OMX_COLOR_FORMATTYPE color_format;
    guint32 fourcc;

    self = GST_OMX_JPEGDEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    {
        OMX_PARAM_PORTDEFINITIONTYPE param;

        /* Input port configuration. */
        {
            G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

            param.format.image.cMIMEType = "OMXJPEGD";
            param.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;

            width = param.format.image.nFrameWidth;
            height = param.format.image.nFrameHeight;
            param.format.image.nSliceHeight = 0;                /*Frame mode , no slice*/
            param.format.image.nStride = 0;
            color_format = param.format.image.eColorFormat;
            fourcc = g_omx_colorformat_to_fourcc (color_format);
            param.nBufferCountActual = 1;

            /* this is against the standard;nBufferSize is read-only. */
            param.nBufferSize = width * height;     /*Avoiding to get a biger image that memory allocated*/

            G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
        }

        /* Output port configuration. */
        {
            G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

            param.format.image.cMIMEType = "OMXJPEGD";
            param.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;

            /*We are configured the output port with the same values
               from input port, except the eColorFormat = NV12*/
            param.nBufferCountActual = 1;
            /*No stride format by default, this maybe change after read the peer caps*/
            param.format.image.nStride      = 0;

            param.format.image.nFrameWidth = width;
            param.format.image.nFrameHeight = height;
            param.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;

            color_format = param.format.image.eColorFormat;
            fourcc = g_omx_colorformat_to_fourcc (color_format);

            /* this is against the standard; nBufferSize is read-only. */
            param.nBufferSize = gst_video_format_get_size_strided (
                      gst_video_format_from_fourcc (fourcc), width, height, 0);

            G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &param);
        }
        self->outport_configured = TRUE;
    }

    /*Set parameters*/
#ifdef OMAP3
    {
        OMX_CUSTOM_RESOLUTION pMaxResolution;
        OMX_INDEXTYPE index;
#if 0
        /*By the moment properties don't have been added */
        OMX_CUSTOM_IMAGE_DECODE_SECTION pSectionDecode;
        OMX_CUSTOM_IMAGE_DECODE_SUBREGION pSubRegionDecode;
        OMX_CONFIG_SCALEFACTORTYPE* pScalefactor;

        /* Section decoding */
        memset (&pSectionDecode, 0, sizeof (pSectionDecode));
        pSectionDecode.nSize = sizeof (OMX_CUSTOM_IMAGE_DECODE_SECTION);

        OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.JPEG.decode.Param.SectionDecode", &index);

        pSectionDecode.nMCURow = 0;
        pSectionDecode.bSectionsInput  = OMX_FALSE;
        pSectionDecode.bSectionsOutput = OMX_TRUE;

        OMX_SetParameter (gomx->omx_handle, index, &pSectionDecode);

        /* SubRegion decoding */
        memset (&pSubRegionDecode, 0, sizeof (pSubRegionDecode));
        pSubRegionDecode.nSize = sizeof (OMX_CUSTOM_IMAGE_DECODE_SUBREGION);

        OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.JPEG.decode.Param.SubRegionDecode", &index);

        pSubRegionDecode.nXOrg = 0;
        pSubRegionDecode.nYOrg  = 0;
        pSubRegionDecode.nXLength = 0;
        pSubRegionDecode.nYLength = 0;

        OMX_SetParameter (gomx->omx_handle, index, &pSubRegionDecode);

        /*scale factor*/

        memset (&pScalefactor, 0, sizeof (pScalefactor));
        pScalefactor.nSize = sizeof (OMX_CONFIG_SCALEFACTORTYPE);

        OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.JPEG.decode.Param.SetMaxResolution", &index);

        pScalefactor.xWidth = (OMX_S32) 100;
        pScalefactor.xHeight = (OMX_S32) 100;

        OMX_SetParameter (gomx->omx_handle, OMX_IndexConfigCommonScale, &pScalefactor);

#endif
        /*Max resolution */
        memset (&pMaxResolution, 0, sizeof (pMaxResolution));

        OMX_GetExtensionIndex (gomx->omx_handle, "OMX.TI.JPEG.decode.Param.SetMaxResolution", &index);

        pMaxResolution.nWidth = width;
        pMaxResolution.nHeight = height;

        OMX_SetParameter (gomx->omx_handle, index, &pMaxResolution);
    }

    /*Set config*/
    {
        OMX_INDEXTYPE index;
        OMX_U32 nProgressive;
        /*Dinamic color change */
        OMX_GetExtensionIndex(gomx->omx_handle, "OMX.TI.JPEG.decode.Config.OutputColorFormat", &index);

        g_assert ( (OMX_SetConfig (gomx->omx_handle, index, &color_format )) == OMX_ErrorNone );

        /*Progressive image decode*/
        OMX_GetExtensionIndex(gomx->omx_handle, "OMX.TI.JPEG.decode.Config.ProgressiveFactor", &index);

        nProgressive= self->progressive;

        OMX_SetConfig(gomx->omx_handle, index, &(nProgressive));

    }
#endif
    GST_INFO_OBJECT (omx_base, "end");
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;
    GstOmxJpegDec *self;

    omx_base = GST_OMX_BASE_FILTER (instance);

    self = GST_OMX_JPEGDEC (instance);

    omx_base->omx_setup = omx_setup;

    omx_base->gomx->settings_changed_cb = settings_changed_cb;

    GST_DEBUG_OBJECT (self, "Setup omx_allocate ports = TRUE; ");
    omx_base->out_port->omx_allocate = TRUE;
    omx_base->out_port->share_buffer = FALSE;

    gst_pad_set_setcaps_function (omx_base->sinkpad, sink_setcaps);

    gst_pad_set_getcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_getcaps));

    gst_pad_set_setcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_setcaps));

    self->framerate_num = 0;
    self->framerate_denom = 1;
    self->progressive = FALSE;
    self->outport_configured = FALSE;

}

