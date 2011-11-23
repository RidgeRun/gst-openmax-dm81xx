#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>


#include "swcsc.h"

GST_DEBUG_CATEGORY_STATIC (gst_swcsc_debug);
#define GST_CAT_DEFAULT gst_swcsc_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ( GST_VIDEO_CAPS_YUV("NV12"))
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ( GST_VIDEO_CAPS_YUV("I420"))
);

static GstElementClass *parent_class = NULL;

static void
 gst_swcsc_base_init(gpointer g_class);
static void
 gst_swcsc_class_init(GstSwcscClass *g_class);
static void
 gst_swcsc_init(GstSwcsc *object);
static gboolean gst_swcsc_exit_colorspace(GstSwcsc *swcsc);
static void gst_swcsc_fixate_caps (GstBaseTransform *trans,
 GstPadDirection direction, GstCaps *caps, GstCaps *othercaps);
static gboolean gst_swcsc_set_caps (GstBaseTransform *trans, 
 GstCaps *in, GstCaps *out);
static GstCaps * gst_swcsc_transform_caps (GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps);
static GstFlowReturn gst_swcsc_transform (GstBaseTransform *trans,
 GstBuffer *inBuf, GstBuffer *outBuf);

static void gst_swcsc_init (GstSwcsc 
    *swcsc)
{
    gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM(swcsc),
     TRUE);
}

GType gst_swcsc_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstSwcscClass),
            gst_swcsc_base_init,
            NULL,
            (GClassInitFunc) gst_swcsc_class_init,
            NULL,
            NULL,
            sizeof(GstSwcsc),
            0,
            (GInstanceInitFunc) gst_swcsc_init
        };

        object_type = g_type_register_static(GST_TYPE_BASE_TRANSFORM,
                          "GstSwcsc", &object_info, 
                            (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_swcsc_debug, 
            "swcsc", 0, " Image colorspace");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};

static void gst_swcsc_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        " Image colorconversion",
        "Filter/Conversion",
        "SW based color conversion ",
        "Brijesh Singh; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
}

static gboolean
gst_transform_event (GstBaseTransform * trans, GstEvent * event)
{
  GstSwcsc *self = GST_SWCSC (trans);

  GST_DEBUG_OBJECT (self, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CROP:
        gst_event_parse_crop (event, &self->crop_top, &self->crop_left,
          &self->crop_width, &self->crop_height);
    default:
      break;
  }

  /* forward all events */
  return TRUE;
}

static void gst_swcsc_class_init(GstSwcscClass 
    *klass)
{
    GObjectClass    *gobject_class;
    GstBaseTransformClass   *trans_class;

    gobject_class    = (GObjectClass*)    klass;
    trans_class      = (GstBaseTransformClass *) klass;

    gobject_class->finalize = 
        (GObjectFinalizeFunc)gst_swcsc_exit_colorspace;

    trans_class->transform_caps = 
        GST_DEBUG_FUNCPTR(gst_swcsc_transform_caps);
    trans_class->set_caps  = 
        GST_DEBUG_FUNCPTR(gst_swcsc_set_caps);
    trans_class->transform = 
        GST_DEBUG_FUNCPTR(gst_swcsc_transform);
    trans_class->fixate_caps = 
        GST_DEBUG_FUNCPTR(gst_swcsc_fixate_caps);
    trans_class->passthrough_on_same_caps = TRUE;
    trans_class->event =
      GST_DEBUG_FUNCPTR (gst_transform_event);
    parent_class = g_type_class_peek_parent (klass);

    GST_LOG("initialized class init\n");
}

static void _convert_420psemi_to_420p (unsigned char * lBuffPtr, unsigned char * cBuffPtr,
    unsigned int xoff, unsigned int yoff, unsigned int ref_width, unsigned int ref_height, unsigned int width,
    unsigned int height, void  *fieldBuf)
{

    unsigned char *CbBuf, *CrBuf, *YBuf, *lumaAddr, *chromaAddr;
    unsigned int pic_size, i, j;

    pic_size = width * height;

    lumaAddr = ( unsigned char *) ((unsigned int) lBuffPtr + (yoff * ref_width) + xoff);
    chromaAddr =  ( unsigned char *) ((unsigned int) cBuffPtr + ((yoff >> 1) * ref_width) + xoff);
    YBuf = (unsigned char *) fieldBuf;

    for (i = 0; i < height; i++) {
        memcpy(YBuf, lumaAddr, width);
        YBuf += width;
        lumaAddr += ref_width;
    }

    CbBuf = (unsigned char *) ((unsigned int)fieldBuf + pic_size);
    CrBuf = (unsigned char *) ((unsigned int)fieldBuf + pic_size + (pic_size >> 2));

    for (i = 0; i < (height >> 1); i++) {
        for (j = 0; j < (width >> 1); j++) {
            CbBuf[j] = chromaAddr[(j * 2)];
            CrBuf[j] = chromaAddr[(j * 2) + 1];
        }
        CbBuf += (width >> 1);
        CrBuf += (width >> 1);
        chromaAddr += ref_width;
    }
}

static void convert_420psemi_to_420p(unsigned char* buffer, int xoffset, int yoffset, int width, 
    int height, int size, unsigned char *output)
{
    int stride, ref_height;
    unsigned char *cptr;

    stride = ((width + (2 * xoffset) + 127) & 0xFFFFFF80);
    ref_height = height + (yoffset << 1);

    cptr = (unsigned char *) ((unsigned long) buffer + ((size / 3) << 1));
    _convert_420psemi_to_420p (buffer, cptr, xoffset, yoffset, stride, ref_height, width, height, output);
}

static GstFlowReturn gst_swcsc_transform (GstBaseTransform *trans,
    GstBuffer *src, GstBuffer *dst)
{
    GstSwcsc *self = GST_SWCSC (trans);

    GST_LOG("begin transform\n");
 
    convert_420psemi_to_420p(GST_BUFFER_DATA(src), self->crop_left, self->crop_top, self->width, self->height, 
        GST_BUFFER_SIZE(src), GST_BUFFER_DATA(dst));

    gst_buffer_set_data (dst, GST_BUFFER_DATA(dst), self->width * self->height * 1.5);

    GST_LOG("end transform\n");
    return GST_FLOW_OK;
}

static GstCaps * gst_swcsc_transform_caps (GstBaseTransform 
 *trans, GstPadDirection direction, GstCaps *from)
{
    GstSwcsc  *swcsc;
    GstCaps *result;
    GstPad *other;
    const GstCaps *templ;

    GST_LOG("begin transform caps (%s)\n",
        direction==GST_PAD_SRC ? "src" : "sink");

    swcsc   = GST_SWCSC(trans);
    g_return_val_if_fail(from != NULL, NULL);

    other = (direction == GST_PAD_SINK) ? trans->srcpad : trans->sinkpad;
    templ = gst_pad_get_pad_template_caps(other);

    result = gst_caps_copy(templ);


    GST_LOG("returing cap %" GST_PTR_FORMAT, result);
    GST_LOG("end transform caps\n");

    return result;
}

static gboolean gst_swcsc_set_caps (GstBaseTransform *trans, 
    GstCaps *in, GstCaps *out)
{
    GstSwcsc *self  = GST_SWCSC(trans);
    gboolean            ret         = TRUE;
    GstVideoFormat in_format;

    GST_LOG("begin set caps\n");

    gst_video_format_parse_caps (in, &in_format, &self->width, &self->height);

    GST_LOG("end set caps\n");
    return ret;
}

static void gst_swcsc_fixate_caps (GstBaseTransform *trans,
     GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
    GstStructure    *outs;
    gint            width, height, framerateNum, framerateDen;
    gboolean        ret;

    g_return_if_fail(gst_caps_is_fixed(caps));

    GST_LOG("begin fixating cap\n");

    ret = gst_video_format_parse_caps(caps, NULL, &width, &height);
    if (!ret) 
        return;

    ret = gst_video_parse_caps_framerate(caps, &framerateNum, &framerateDen);
    if (!ret) 
        return;

    outs = gst_caps_get_structure(othercaps, 0);
    gst_structure_fixate_field_nearest_int (outs, "width", width);
    gst_structure_fixate_field_nearest_int (outs, "height", height);
    gst_structure_fixate_field_nearest_fraction (outs, "framerate", 
        framerateNum, framerateDen);

    GST_LOG("end fixating cap\n");
}

static gboolean gst_swcsc_exit_colorspace(GstSwcsc *swcsc)
{
    GST_LOG("begin exit_video\n");

    return TRUE;
}

