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

#ifndef GSTOMX_H263ENC_H
#define GSTOMX_H263ENC_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_H263ENC(obj) (GstOmxH263Enc *) (obj)
#define GST_OMX_H263ENC_TYPE (gst_omx_h263enc_get_type ())

typedef struct GstOmxH263Enc GstOmxH263Enc;
typedef struct GstOmxH263EncClass GstOmxH263EncClass;

#include "gstomx_base_videoenc.h"

struct GstOmxH263Enc
{
    GstOmxBaseVideoEnc omx_base;
    OMX_VIDEO_H263PROFILETYPE profile;
    OMX_VIDEO_H263LEVELTYPE level;
};

struct GstOmxH263EncClass
{
    GstOmxBaseVideoEncClass parent_class;
};

GType gst_omx_h263enc_get_type (void);

G_END_DECLS

#endif /* GSTOMX_H263ENC_H */
