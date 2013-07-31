/*
 * Copyright (C) 2008-2009 Nokia Corporation.
 *
 * Author: David Soto <david.soto@ridgerun.com>
 * Copyright (C) 2013 RidgeRun
 *
 * Based on the plugin version created by: 
 *            Felipe Contreras <felipe.contreras@nokia.com>
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

#ifndef GSTOMX_MJPEGENC_H
#define GSTOMX_MJPEGENC_H

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_OMX_MJPEGENC(obj) (GstOmxMjpegEnc *) (obj)
#define GST_OMX_MJPEGENC_TYPE (gst_omx_mjpegenc_get_type ())
typedef struct GstOmxMjpegEnc GstOmxMjpegEnc;
typedef struct GstOmxMjpegEncClass GstOmxMjpegEncClass;

#include "gstomx_base_videoenc.h"

struct GstOmxMjpegEnc
{
  GstOmxBaseVideoEnc omx_base;
  gint quality;
};

struct GstOmxMjpegEncClass
{
  GstOmxBaseVideoEncClass parent_class;
};

GType gst_omx_mjpegenc_get_type (void);

G_END_DECLS
#endif /* GSTOMX_MJPEGENC_H */
