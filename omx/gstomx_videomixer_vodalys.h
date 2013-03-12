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

#ifndef GSTOMX_VIDEOMIXER_VODALYS_H
#define GSTOMX_VIDEOMIXER_VODALYS_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_VIDEOMIXER_VODALYS(obj) (GstOmxVideoMixerVodalys *) (obj)
#define GST_OMX_VIDEOMIXER_VODALYS_TYPE (gst_omx_videomixer_vodalys_get_type ())

typedef struct GstOmxVideoMixerVodalys GstOmxVideoMixerVodalys;
typedef struct GstOmxVideoMixerVodalysClass GstOmxVideoMixerVodalysClass;

#include "gstomx_base_filter21.h"
#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
struct GstOmxVideoMixerVodalys
{
    GstOmxBaseFilter21 omx_base;
};

struct GstOmxVideoMixerVodalysClass
{
    GstOmxBaseFilter21Class parent_class;
};

GType gst_omx_videomixer_vodalys_get_type (void);

G_END_DECLS

#endif /* GSTOMX_DUMMY_H */
