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

#include "gstomx.h"
#include "gstomx_dummy.h"
#include "gstomx_mpeg4dec.h"
#include "gstomx_mpeg2dec.h"
#include "gstomx_h263dec.h"
#include "gstomx_h264dec.h"
#include "gstomx_vp6dec.h"
#include "gstomx_wmvdec.h"
#include "gstomx_mpeg4enc.h"
#include "gstomx_h264enc.h"
#include "gstomx_h263enc.h"
#include "gstomx_vorbisdec.h"
#include "gstomx_mp3dec.h"
#include "gstomx_mp2dec.h"
#include "gstomx_aacdec.h"
#include "gstomx_aacenc.h"
#include "gstomx_amrnbdec.h"
#include "gstomx_amrnbenc.h"
#include "gstomx_amrwbdec.h"
#include "gstomx_amrwbenc.h"
#include "gstomx_adpcmdec.h"
#include "gstomx_adpcmenc.h"
#include "gstomx_g711dec.h"
#include "gstomx_g711enc.h"
#include "gstomx_g729dec.h"
#include "gstomx_g729enc.h"
#include "gstomx_ilbcdec.h"
#include "gstomx_ilbcenc.h"
#include "gstomx_jpegenc.h"
#include "gstomx_jpegdec.h"
#include "gstomx_audiosink.h"
#include "gstomx_videosink.h"
#include "gstomx_filereadersrc.h"
#include "gstomx_volume.h"
#include "gstomx_camera.h"
#include "swcsc.h"
#include "gstperf.h"
#include "gstomx_scaler.h"
#include "gstomx_noisefilter.h"
#include "gstomx_base_ctrl.h"
#include "gstomx_vc1dec.h"

#include "gstomx_videomixer.h"
#include "config.h"

GST_DEBUG_CATEGORY (gstomx_debug);
GST_DEBUG_CATEGORY (gstomx_ppm);

typedef struct TableItem
{
    const gchar *name;
    const gchar *library_name;
    const gchar *component_name;
    const gchar *component_role;
    guint rank;
    GType (*get_type) (void);
} TableItem;

static TableItem element_table[] =
{
//    { "omx_dummy",          "libOMX_Core.so",           "OMX.TI.DUCATI1.MISC.SAMPLE",   NULL,                   GST_RANK_NONE,      gst_omx_dummy_get_type },
    { "omx_mpeg4dec",       "libOMX_Core.so",           "OMX.TI.DUCATI.VIDDEC", "",  GST_RANK_PRIMARY,   gst_omx_mpeg4dec_get_type },
    { "omx_h264dec",        "libOMX_Core.so",           "OMX.TI.DUCATI.VIDDEC", "",    GST_RANK_PRIMARY,   gst_omx_h264dec_get_type },
    { "omx_mpeg2dec",       "libOMX_Core.so",           "OMX.TI.DUCATI.VIDDEC", "",  GST_RANK_PRIMARY,   gst_omx_mpeg2dec_get_type },
//    { "omx_h263dec",        "libOMX_Core.so",           "OMX.TI.DUCATI.VIDDEC", "",   GST_RANK_PRIMARY,   gst_omx_h263dec_get_type },
//    { "omx_vp6dec",         "libOMX_Core.so",           "OMX.TI.DUCATI1.VIDEO.DECODER", "video_decoder.vp6",    GST_RANK_PRIMARY,   gst_omx_vp6dec_get_type },
//    { "omx_wmvdec",         "libOMX_Core.so",           "OMX.TI.Video.Decoder",         NULL,                   GST_RANK_NONE,      gst_omx_wmvdec_get_type },
//    { "omx_mpeg4enc",       "libOMX_Core.so",           "OMX.TI.DUCATI.VIDENC",  NULL,                   GST_RANK_PRIMARY,   gst_omx_mpeg4enc_get_type },
    { "omx_h264enc",        "libOMX_Core.so",           "OMX.TI.DUCATI.VIDENC",   "",                   GST_RANK_PRIMARY,   gst_omx_h264enc_get_type },
    { "omx_vc1dec",        "libOMX_Core.so",           "OMX.TI.DUCATI.VIDDEC",   "",                   GST_RANK_PRIMARY,   gst_omx_vc1dec_get_type },
//    { "omx_h263enc",        "libOMX_Core.so",           "OMX.TI.DUCATI.VIDENC",  NULL,                   GST_RANK_PRIMARY,   gst_omx_h263enc_get_type },
//    { "omx_vorbisdec",      "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.ogg.single", NULL,                GST_RANK_NONE,   gst_omx_vorbisdec_get_type },
//    { "omx_mp3dec",         "libOMX_Core.so",           "OMX.TI.AUDIO.DECODE",          "audio_decode.dsp.mp3", GST_RANK_NONE,   gst_omx_mp3dec_get_type },
//    { "omx_mp2dec",         "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.mp3.mad", NULL,                   GST_RANK_NONE,   gst_omx_mp2dec_get_type },
//    { "omx_amrnbdec",       "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.amrnb",   NULL,                   GST_RANK_NONE,   gst_omx_amrnbdec_get_type },
//    { "omx_amrnbenc",       "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.amrnb",   NULL,                   GST_RANK_NONE,   gst_omx_amrnbenc_get_type },
//    { "omx_amrwbdec",       "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.amrwb",   NULL,                   GST_RANK_NONE,   gst_omx_amrwbdec_get_type },
//    { "omx_amrwbenc",       "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.amrwb",   NULL,                   GST_RANK_NONE,   gst_omx_amrwbenc_get_type },
//    { "omx_aacdec",         "libOMX_Core.so",           "OMX.TI.AUDIO.DECODE",          "audio_decode.dsp.aac", GST_RANK_NONE,   gst_omx_aacdec_get_type },
//    { "omx_aacenc",         "libOMX_Core.so",           "OMX.TI.AUDIO.ENCODE",          "audio_encode.dsp.aac", GST_RANK_NONE,   gst_omx_aacenc_get_type },
//    { "omx_adpcmdec",       "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.adpcm",   NULL,                   GST_RANK_NONE,   gst_omx_adpcmdec_get_type },
//    { "omx_adpcmenc",       "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.adpcm",   NULL,                   GST_RANK_NONE,   gst_omx_adpcmenc_get_type },
//    { "omx_g711dec",        "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.g711",    NULL,                   GST_RANK_NONE,   gst_omx_g711dec_get_type },
//    { "omx_g711enc",        "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.g711",    NULL,                   GST_RANK_NONE,   gst_omx_g711enc_get_type },
//    { "omx_g729dec",        "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.g729",    NULL,                   GST_RANK_NONE,   gst_omx_g729dec_get_type },
//    { "omx_g729enc",        "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.g729",    NULL,                   GST_RANK_NONE,   gst_omx_g729enc_get_type },
//    { "omx_ilbcdec",        "libomxil-bellagio.so.0",   "OMX.st.audio_decoder.ilbc",    NULL,                   GST_RANK_NONE,   gst_omx_ilbcdec_get_type },
//    { "omx_ilbcenc",        "libomxil-bellagio.so.0",   "OMX.st.audio_encoder.ilbc",    NULL,                   GST_RANK_NONE,   gst_omx_ilbcenc_get_type },
//    { "omx_jpegenc",        "libOMX_Core.so",           "OMX.TI.JPEG.encoder",          NULL,                   GST_RANK_NONE,   gst_omx_jpegenc_get_type },
//    { "omx_jpegdec",        "libOMX_Core.so",           "OMX.TI.DUCATI1.IMAGE.JPEGD",   NULL,                   GST_RANK_NONE,   gst_omx_jpegdec_get_type },
//    { "omx_audiosink",      "libomxil-bellagio.so.0",   "OMX.st.alsa.alsasink",         NULL,                   GST_RANK_NONE,      gst_omx_audiosink_get_type },
    { "omx_videosink",      "libOMX_Core.so",   "OMX.TI.VPSSM3.VFDC",             NULL,              GST_RANK_PRIMARY,      gst_omx_videosink_get_type },
//    { "omx_filereadersrc",  "libomxil-bellagio.so.0",   "OMX.st.audio_filereader",      NULL,                   GST_RANK_NONE,      gst_omx_filereadersrc_get_type },
//    { "omx_volume",         "libomxil-bellagio.so.0",   "OMX.st.volume.component",      NULL,                   GST_RANK_NONE,      gst_omx_volume_get_type },
    { "swcsc",         "libOMX_Core.so",   NULL,      NULL,                   GST_RANK_PRIMARY,      gst_swcsc_get_type },
    { "gstperf",         "libOMX_Core.so",   NULL,      NULL,                   GST_RANK_PRIMARY,      gst_perf_get_type },
    { "omx_scaler",         "libOMX_Core.so",   "OMX.TI.VPSSM3.VFPC.INDTXSCWB",     "",                   GST_RANK_PRIMARY,      gst_omx_scaler_get_type },
    { "omx_noisefilter",         "libOMX_Core.so",   "OMX.TI.VPSSM3.VFPC.NF",     "",                   GST_RANK_PRIMARY,      gst_omx_noisefilter_get_type },
    { "omx_ctrl",         "libOMX_Core.so",   "OMX.TI.VPSSM3.CTRL.DC",     "",                   GST_RANK_PRIMARY,      gst_omx_base_ctrl_get_type },
//    { "omx_camera",         "libOMX_Core.so",           "OMX.TI.DUCATI1.VIDEO.CAMERA",  NULL,                   GST_RANK_PRIMARY,   gst_omx_camera_get_type },
	{ "omx_videomixer", 		"libOMX_Core.so",	"OMX.TI.VPSSM3.VFPC.INDTXSCWB", 	"", 				  GST_RANK_PRIMARY, 	 gst_omx_video_mixer_get_type },
    { NULL, NULL, NULL, NULL, 0, NULL },
};

static gboolean
plugin_init (GstPlugin *plugin)
{
    GQuark library_name_quark;
    GQuark component_name_quark;
    GQuark component_role_quark;
    GST_DEBUG_CATEGORY_INIT (gstomx_debug, "omx", 0, "gst-openmax");
    GST_DEBUG_CATEGORY_INIT (gstomx_util_debug, "omx_util", 0, "gst-openmax utility");
    GST_DEBUG_CATEGORY_INIT (gstomx_ppm, "omx_ppm", 0,
                             "gst-openmax performance");

    library_name_quark = g_quark_from_static_string ("library-name");
    component_name_quark = g_quark_from_static_string ("component-name");
    component_role_quark = g_quark_from_static_string ("component-role");

    g_omx_init ();

    {
        guint i;
        for (i = 0; element_table[i].name; i++)
        {
            TableItem *element;
            GType type;

            element = &element_table[i];
            type = element->get_type ();
            g_type_set_qdata (type, library_name_quark, (gpointer) element->library_name);
            g_type_set_qdata (type, component_name_quark, (gpointer) element->component_name);
            g_type_set_qdata (type, component_role_quark, (gpointer) element->component_role);

            if (!gst_element_register (plugin, element->name, element->rank, type))
            {
                g_warning ("failed registering '%s'", element->name);
                return FALSE;
            }
        }
    }

    return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "omx",
                   "OpenMAX IL",
                   plugin_init,
                   PACKAGE_VERSION,
                   GST_LICENSE,
                   GST_PACKAGE_NAME,
                   GST_PACKAGE_ORIGIN)
