/*
 * Ridgerun
 * Main Author:
 * 	2012 Luis Arce  <luis.arce@ridgerun.com>
 * Checked by
 *  2012 David Soto <david.soto@ridgerun.com>
 * Based on the code created by Diego Dompe in the dmai plugins
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include "gstomx_rrparser.h"

GST_DEBUG_CATEGORY_STATIC (gst_rrparser_debug);
#define GST_CAT_DEFAULT gst_rrparser_debug
#define NAL_LENGTH 4


GST_BOILERPLATE (GstLegacyRRParser, gst_rrparser, GstElement,
    GST_TYPE_ELEMENT);

/* Properties */
enum
{
  PROP_0,
  SINGLE_NALU,
};

/*
 * The capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
						"stream-format = (string) byte-stream,"
						"width = (int) [ 1, MAX ],"
						"height = (int) [ 1, MAX ],"
						"framerate=(fraction)[ 0, MAX ];"
	)
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
						"stream-format = (string) avc,"
						"width = (int) [ 1, MAX ],"
						"height = (int) [ 1, MAX ],"
						"framerate=(fraction)[ 0, MAX ];"
	)
    );

static void gst_rrparser_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rrparser_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rrparser_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_rrparser_chain (GstPad * pad, GstBuffer * buf);


static void
gst_rrparser_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "rr_h264parser",
    "H264 parser (bytestream to Nal Stream)",
    "H264 parser (bytestream to Nal Stream)",
    "Luis Fernando Arce; RidgeRun Engineering");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_rrparser_class_init (GstLegacyRRParserClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_rrparser_set_property;
  gobject_class->get_property = gst_rrparser_get_property;

  g_object_class_install_property (gobject_class, SINGLE_NALU,
      g_param_spec_boolean ("singleNalu", "SingleNalu", "Buffers are single Nal units",
          FALSE, G_PARAM_READWRITE));
}

static gboolean gst_rrparser_sink_event(GstPad *pad, GstEvent *event)
{
    GstLegacyRRParser * rrparser =(GstLegacyRRParser *) gst_pad_get_parent(pad);
	gboolean ret = FALSE;

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
		case GST_EVENT_EOS:
			ret = gst_pad_push_event(rrparser->src_pad, event);
			break;
		default:
			ret = gst_pad_push_event(rrparser->src_pad, event);
    }
    return ret;
}

static void
gst_rrparser_init (GstLegacyRRParser * rrparser,
    GstLegacyRRParserClass * gclass)
{

  rrparser->set_codec_data = FALSE;
  rrparser->SPS_PPS_end = -1;
  rrparser->PPS_start = -1;
  rrparser->single_Nalu = FALSE;

  rrparser->sink_pad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (rrparser->sink_pad,
                                GST_DEBUG_FUNCPTR(gst_rrparser_set_caps));
  gst_pad_set_chain_function   (rrparser->sink_pad,
                              GST_DEBUG_FUNCPTR(gst_rrparser_chain));
  gst_pad_set_event_function(
        rrparser->sink_pad, GST_DEBUG_FUNCPTR(gst_rrparser_sink_event));

  rrparser->src_pad = gst_pad_new_from_static_template (&src_factory, "src");

  gst_element_add_pad (GST_ELEMENT (rrparser), rrparser->sink_pad);
  gst_element_add_pad (GST_ELEMENT (rrparser), rrparser->src_pad);

}

static void
gst_rrparser_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  GstLegacyRRParser *rrparser = (GstLegacyRRParser *)object;

  switch (prop_id) {
    case SINGLE_NALU:
      rrparser->single_Nalu = g_value_get_boolean(value);
      break;
    default:
      break;
  }
}

static void
gst_rrparser_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{

  GstLegacyRRParser *rrparser = (GstLegacyRRParser *)object;

  switch (prop_id) {
	case SINGLE_NALU:
	  g_value_set_boolean(value, rrparser->single_Nalu);
      break;
    default:
      break;
  }
}

GstCaps*
gst_rrparser_fixate_src_caps(GstLegacyRRParser *rrparser, GstCaps *filter_caps){

  GstCaps *caps, *othercaps;

  GstStructure *structure;
  GstStructure *filter_structure;
  const gchar *stream_format;

  int filter_width = 0;
  int filter_height = 0;
  int filter_framerateN = 0;
  int filter_framerateD = 0;

  GST_DEBUG_OBJECT (rrparser, "Enter fixate_src_caps");

  /* Obtain the intersec between the src_pad and this peer caps */
  othercaps = gst_pad_get_allowed_caps(rrparser->src_pad);

  if (othercaps == NULL ||
      gst_caps_is_empty (othercaps) || gst_caps_is_any (othercaps)) {
    /* If we got nothing useful, user our template caps */
    caps =
        gst_caps_copy (gst_pad_get_pad_template_caps (rrparser->src_pad));
  } else {
    /* We got something useful */
    caps = othercaps;
  }

  /* Ensure that the caps are writable */
  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);
  if (structure == NULL) {
    GST_ERROR_OBJECT (rrparser, "Failed to get src caps structure");
    return NULL;
  }

  /* Force to use avc and nal in case of null */
  stream_format = gst_structure_get_string (structure, "stream-format");
  if (stream_format == NULL) {
    stream_format = "avc";
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, stream_format, (char *)NULL);
  }

  /* Get caps filter fields */
  filter_structure = gst_caps_get_structure (filter_caps, 0);
  gst_structure_get_fraction(filter_structure, "framerate", &filter_framerateN,
							&filter_framerateD);
  gst_structure_get_int(filter_structure, "height", &filter_height);
  gst_structure_get_int(filter_structure, "width", &filter_width);

  /* Set the width, height and framerate */
  gst_structure_set (structure, "width", G_TYPE_INT,
					 filter_width, (char *)NULL);
  gst_structure_set (structure, "height", G_TYPE_INT,
					 filter_height, (char *)NULL);
  gst_structure_set (structure, "framerate", GST_TYPE_FRACTION, filter_framerateN,
					 filter_framerateD, (char *)NULL);


  GST_DEBUG_OBJECT (rrparser, "Leave fixate_src_caps");
  return caps;
}

static gboolean
gst_rrparser_set_caps (GstPad * pad, GstCaps * caps)
{
  const gchar *mime;
  const gchar *stream_format;
  GstCaps *src_caps;
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstLegacyRRParser *rrparser = (GstLegacyRRParser *)gst_pad_get_parent(pad);

  mime = gst_structure_get_name (structure);
  stream_format = gst_structure_get_string (structure, "stream-format");

  /* Check mime type */
  if ((mime != NULL) && (strcmp (mime, "video/x-h264") != 0)) {
	GST_WARNING ("Wrong mimetype %s provided, we only support %s",
			    mime, "video/x-h264");

	goto refuse_caps;
  }

  /* Check for the stream format */
  if((stream_format != NULL) && (strcmp (stream_format, "byte-stream") != 0)) {
	GST_WARNING ("Wrong stream-format %s provided, we only support %s",
			    stream_format, "byte-stream");

	goto refuse_caps;
  }

  /* Obtain a fixed src caps and set it for the src pad */
  src_caps = gst_rrparser_fixate_src_caps(rrparser, caps);
  if(NULL == src_caps) {
	GST_WARNING("Can't fixate src caps");
	goto refuse_caps;
  }

  if(!gst_pad_set_caps (rrparser->src_pad, src_caps)) {
	GST_WARNING("Can't setc src pad");
	goto refuse_caps;
  }

  return TRUE;

    /* ERRORS */
refuse_caps:
{
  GST_ERROR ("refused caps %" GST_PTR_FORMAT, caps);

  return FALSE;
}

}


/* This function searchs for specific NAL types */
GstBuffer*
gst_rrparser_fetch_nal(GstBuffer *buffer, gint type)
{

	gint i;
    guchar *data = GST_BUFFER_DATA(buffer);
    GstBuffer *nal_buffer;
    gint nal_idx = 0;
    gint nal_len = 0;
    gint nal_type = 0;
    gint found = 0;
    gint done = 0;

    GST_DEBUG("Fetching NAL, type %d", type);
    for (i = 0; i < GST_BUFFER_SIZE(buffer) - 5; i++) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0
            && data[i + 3] == 1) {
            if (found == 1) {
                nal_len = i - nal_idx;
                done = 1;
                break;
            }

            nal_type = (data[i + 4]) & 0x1f;
            if (nal_type == type)
            {
                found = 1;
                nal_idx = i + 4;
                i += 4;
            }
        }
    }

    /* Check if the NAL stops at the end */
    if (found == 1 && done != 0 && i >= GST_BUFFER_SIZE(buffer) - 4) {
        nal_len = GST_BUFFER_SIZE(buffer) - nal_idx;
        done = 1;
    }

    if (done == 1) {
        GST_DEBUG("Found NAL, bytes [%d-%d] len [%d]", nal_idx, nal_idx + nal_len - 1, nal_len);
        nal_buffer = gst_buffer_new_and_alloc(nal_len);
        memcpy(GST_BUFFER_DATA(nal_buffer),&data[nal_idx],nal_len);
        return nal_buffer;
    } else {
        GST_DEBUG("Did not find NAL type %d", type);
        return NULL;
    }
}


/* This function creates the buffer with the SPS and PPS information */
GstBuffer*
gst_rrparser_generate_codec_data(GstLegacyRRParser *rrparser, GstBuffer *buffer) {

	GstBuffer *avcc = NULL;
    guchar *avcc_data = NULL;
    gint avcc_len = 7;  // Default 7 bytes w/o SPS, PPS data
    gint i;

    GstBuffer *sps = NULL;
    guchar *sps_data = NULL;
    gint num_sps=0;

    GstBuffer *pps = NULL;
    gint num_pps=0;

    guchar profile;
    guchar compatibly;
    guchar level;

    sps = gst_rrparser_fetch_nal(buffer, 7); // 7 = SPS
    if (sps){
        num_sps = 1;
        avcc_len += GST_BUFFER_SIZE(sps) + 2;
        sps_data = GST_BUFFER_DATA(sps);

        profile     = sps_data[1];
        compatibly  = sps_data[2];
        level       = sps_data[3];

        GST_DEBUG("SPS: profile=%d, compatibly=%d, level=%d",
                    profile, compatibly, level);
    } else {
        GST_WARNING("No SPS found");

        profile     = 1;      // Default Profile: Baseline
        compatibly  = 0;
        level       = 8192;   // Default Level: 4.2
    }
    pps = gst_rrparser_fetch_nal(buffer, 8); // 8 = PPS
    if (pps){
        num_pps = 1;
        avcc_len += GST_BUFFER_SIZE(pps) + 2;
    }

	/* Since we already know the position of the SPS and PPS we save these values */
	rrparser->SPS_PPS_end = NAL_LENGTH + GST_BUFFER_SIZE(sps) + NAL_LENGTH + GST_BUFFER_SIZE(pps);
	rrparser->PPS_start = NAL_LENGTH + GST_BUFFER_SIZE(sps) + NAL_LENGTH;

    avcc = gst_buffer_new_and_alloc(avcc_len);
    avcc_data = GST_BUFFER_DATA(avcc);
    avcc_data[0] = 1;               // [0] 1 byte - version
    avcc_data[1] = profile;         // [1] 1 byte - h.264 stream profile
    avcc_data[2] = compatibly;      // [2] 1 byte - h.264 compatible profiles
    avcc_data[3] = level;           // [3] 1 byte - h.264 stream level
    avcc_data[4] = 0xfc | (NAL_LENGTH-1);  // [4] 6 bits - reserved all ONES = 0xfc
                                    // [4] 2 bits - NAL length ( 0 - 1 byte; 1 - 2 bytes; 3 - 4 bytes)
    avcc_data[5] = 0xe0 | num_sps;  // [5] 3 bits - reserved all ONES = 0xe0
                                    // [5] 5 bits - number of SPS
    i = 6;
    if (num_sps > 0){
        avcc_data[i++] = GST_BUFFER_SIZE(sps) >> 8;
        avcc_data[i++] = GST_BUFFER_SIZE(sps) & 0xff;
        memcpy(&avcc_data[i],GST_BUFFER_DATA(sps),GST_BUFFER_SIZE(sps));
        i += GST_BUFFER_SIZE(sps);
    }
    avcc_data[i++] = num_pps;      // [6] 1 byte  - number of PPS
    if (num_pps > 0){
        avcc_data[i++] = GST_BUFFER_SIZE(pps) >> 8;
        avcc_data[i++] = GST_BUFFER_SIZE(pps) & 0xff;
        memcpy(&avcc_data[i],GST_BUFFER_DATA(pps),GST_BUFFER_SIZE(pps));
        i += GST_BUFFER_SIZE(sps);
    }

    return avcc;
}

/* This function sets the codec data (SPS and PPS) in the src_pad caps */
gboolean
gst_rrparser_set_codec_data(GstLegacyRRParser *rrparser, GstBuffer *buf){

  GstBuffer *codec_data;
  GstCaps *src_caps;

  GST_DEBUG("Entry gst_rrparser_set_codec_data");

  /* Generate the codec data with the SPS and the PPS */
  codec_data = gst_rrparser_generate_codec_data(rrparser, buf);

  /* Update the caps with the codec data */
  src_caps = gst_caps_make_writable(gst_caps_ref(GST_PAD_CAPS(rrparser->src_pad)));
  gst_caps_set_simple (src_caps, "codec_data", GST_TYPE_BUFFER, codec_data, (char *)NULL);
  if (!gst_pad_set_caps (rrparser->src_pad, src_caps)) {
	  GST_WARNING_OBJECT (rrparser, "Src caps can't be updated");
  }

  gst_buffer_unref (codec_data);

    GST_DEBUG("Leave gst_rrparser_set_codec_data");

  return TRUE;
}

/* This function does the real work, converts from bystream to NAL stream */
GstBuffer*
gst_rrparser_to_packetized(GstLegacyRRParser *rrparser, GstBuffer *out_buffer) {

    GST_DEBUG("Entry gst_rrparser_to_packetized");

    gint i, mark = 0, nal_type = -1;
    gint test_sps_type = -1, test_pps_type = -1;
    gint size = GST_BUFFER_SIZE(out_buffer);
    guchar *dest;

	dest = GST_BUFFER_DATA(out_buffer);

	for (i = 0; i < size - 4; i++) {
        if (dest[i] == 0 && dest[i + 1] == 0 &&
            dest[i+2] == 0 && dest[i + 3] == 1) {

			if(rrparser->single_Nalu){
				test_sps_type = (dest[i + NAL_LENGTH]) & 0x1f;
				test_pps_type = (dest[i + rrparser->PPS_start]) & 0x1f;
				/* If we found a I frame */
				if ((test_sps_type == 7) && (test_pps_type == 8)) {
					GST_DEBUG("Single-NALU: we found a I-frame");
					mark = (i + rrparser->SPS_PPS_end) + NAL_LENGTH;
					GST_BUFFER_DATA(out_buffer) = &dest[i + rrparser->SPS_PPS_end];
	                GST_BUFFER_SIZE(out_buffer) = size - (i + rrparser->SPS_PPS_end);
			GST_BUFFER_FLAG_UNSET (out_buffer,
                            GST_BUFFER_FLAG_DELTA_UNIT);
				}
				/* We found a P frame */
				else
				{
					GST_DEBUG("Single-NALU: we found a P-frame");
					mark = i + NAL_LENGTH;
					nal_type = (dest[i + NAL_LENGTH]) & 0x1f;
					GST_BUFFER_FLAG_SET (out_buffer,
	         			     GST_BUFFER_FLAG_DELTA_UNIT);
				}
				i = (size - 4);
				break;
			}
			else {
	            /* Do not copy if current NAL is nothing (this is the first start code) */
	            if (nal_type == -1) {
	                nal_type = (dest[i + 4]) & 0x1f;
	            } else {
	                if (nal_type == 7 || nal_type == 8) {
						/* Discard anything previous to the SPS and PPS */
						GST_BUFFER_DATA(out_buffer) = &dest[i];
						GST_BUFFER_SIZE(out_buffer) = size - i;
						GST_DEBUG("SPS and PPS discard");
					} else {
						/* Replace the NAL start code with the length */
						gint length = i - mark ;
						gint k;
						for (k = 1 ; k <= 4; k++){
							dest[mark - k] = length & 0xff;
							length >>= 8;
						}

						nal_type = (dest[i + 4]) & 0x1f;
					}
				}
	            /* Mark where next NALU starts */
	            mark = i + 4;

	            nal_type = (dest[i + 4]) & 0x1f;
	        }
		}

	}


    if (i == (size - 4)){
        /* We reach the end of the buffer */
        if ((nal_type != -1) || ((test_sps_type == 7) && (test_pps_type == 8))){
            /* Replace the NAL start code with the length */
            gint length = size - mark ;
            gint k;
            GST_DEBUG("Replace the NAL start code with the NAL length");
            for (k = 1 ; k <= 4; k++){
                dest[mark - k] = length & 0xff;
                length >>= 8;
            }
        }
    }

	GST_DEBUG("Leave gst_rrparser_to_packetized");

    return out_buffer;
}

static GstFlowReturn
gst_rrparser_chain (GstPad *pad, GstBuffer *buf)
{
  GstLegacyRRParser *rrparser = GST_RRPARSER (GST_OBJECT_PARENT (pad));
  GstFlowReturn ret;
  GST_DEBUG("Entry gst_rrparser_chain");

  /* Obtain and set codec data */
  if(!rrparser->set_codec_data) {
	if(!gst_rrparser_set_codec_data(rrparser, buf)) {
		GST_WARNING("Problems for generate codec data");
	}
	rrparser->set_codec_data = TRUE;
  }

  /* Change the buffer content to packetizer */
  gst_rrparser_to_packetized(rrparser, buf);

  /* Set the caps of the buffer */
  gst_caps_unref(GST_BUFFER_CAPS (buf));
  GST_BUFFER_CAPS (buf) = gst_caps_ref(GST_PAD_CAPS(rrparser->src_pad));

  ret = gst_pad_push (rrparser->src_pad, buf);

  GST_DEBUG("Leave gst_rrparser_chain");

  return ret;

}
