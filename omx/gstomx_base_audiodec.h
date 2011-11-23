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

#ifndef GSTOMX_BASE_AUDIODEC_H
#define GSTOMX_BASE_AUDIODEC_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_BASE_AUDIODEC(obj) (GstOmxBaseAudioDec *) (obj)
#define GST_OMX_BASE_AUDIODEC_TYPE (gst_omx_base_audiodec_get_type ())

typedef struct GstOmxBaseAudioDec GstOmxBaseAudioDec;
typedef struct GstOmxBaseAudioDecClass GstOmxBaseAudioDecClass;

#include "gstomx_base_filter.h"

struct GstOmxBaseAudioDec
{
    GstOmxBaseFilter omx_base;
    gint rate;
    gint channels;
};

struct GstOmxBaseAudioDecClass
{
    GstOmxBaseFilterClass parent_class;
};

GType gst_omx_base_audiodec_get_type (void);

G_END_DECLS

#endif /* GSTOMX_BASE_AUDIODEC_H */
