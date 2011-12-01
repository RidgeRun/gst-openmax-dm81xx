/*
 * Copyright (C) 2009 Texas Instruments, Inc - http://www.ti.com/
 *
 * Description: Base audio decoder element
 *  Created on: Aug 2, 2009
 *      Author: Rob Clark <rob@ti.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "gstomx_base_audiodec.h"
#include "gstomx.h"

#include <string.h> /* for memset */
#define CHANNELS_DEFAULT 2
#define SAMPLERATE_DEFAULT 44100

GSTOMX_BOILERPLATE (GstOmxBaseAudioDec, gst_omx_base_audiodec, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);

static void
type_base_init (gpointer g_class)
{
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
    GstStructure *structure;
    GstOmxBaseFilter *omx_base;
    GstOmxBaseAudioDec *self;
    GOmxCore *gomx;

    omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    self = GST_OMX_BASE_AUDIODEC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;
    GST_INFO_OBJECT (omx_base, "getcaps (sink): %" GST_PTR_FORMAT, caps);

    self->rate = SAMPLERATE_DEFAULT;
    self->channels = CHANNELS_DEFAULT;

    {
        g_return_val_if_fail (caps, FALSE);
        g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

        structure = gst_caps_get_structure (caps, 0);

        gst_structure_get_int (structure, "rate", &self->rate);
        gst_structure_get_int (structure, "channels", &self->channels);
    }

    return gst_pad_set_caps (pad, caps);
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseFilter *omx_base;
    guint rate;
    guint channels;

    omx_base = core->object;

    GST_DEBUG_OBJECT (omx_base, "settings changed");

    {
        OMX_AUDIO_PARAM_PCMMODETYPE param;

        G_OMX_PORT_GET_PARAM (omx_base->out_port, OMX_IndexParamAudioPcm, &param);

        rate = param.nSamplingRate;
        channels = param.nChannels;

        if (rate == 0)
        {
            /** @todo: this shouldn't happen. */
            GST_WARNING_OBJECT (omx_base, "Bad samplerate");
            rate = 44100;
            channels = 2;
        }
    }

    {
        GstCaps *new_caps;

        new_caps = gst_caps_new_simple ("audio/x-raw-int",
                                        "width", G_TYPE_INT, 16,
                                        "depth", G_TYPE_INT, 16,
                                        "rate", G_TYPE_INT, rate,
                                        "signed", G_TYPE_BOOLEAN, TRUE,
                                        "endianness", G_TYPE_INT, G_BYTE_ORDER,
                                        "channels", G_TYPE_INT, channels,
                                        NULL);

        GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
        gst_pad_set_caps (omx_base->srcpad, new_caps);
    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;

    omx_base = GST_OMX_BASE_FILTER (instance);

    GST_DEBUG_OBJECT (omx_base, "start");

    omx_base->gomx->settings_changed_cb = settings_changed_cb;

    gst_pad_set_setcaps_function (omx_base->sinkpad,
                                  (sink_setcaps));
}
