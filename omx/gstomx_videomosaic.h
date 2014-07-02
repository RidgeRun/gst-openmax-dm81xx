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

#ifndef GSTOMX_VIDEO_MOSAIC_H
#define GSTOMX_VIDEO_MOSAIC_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_VIDEO_MOSAIC(obj) ((GstOmxVideoMosaic *) (obj))
#define GST_OMX_VIDEO_MOSAIC_TYPE (gst_omx_video_mosaic_get_type ())

typedef struct GstOmxVideoMosaic GstOmxVideoMosaic;
typedef struct GstOmxVideoMosaicClass GstOmxVideoMosaicClass;

#include "gstomx_base_filter21.h"
#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
struct GstOmxVideoMosaic
{
    GstOmxBaseFilter21 omx_base;
};

struct GstOmxVideoMosaicClass
{
    GstOmxBaseFilter21Class parent_class;
};

GType gst_omx_video_mosaic_get_type (void);

G_END_DECLS

#endif /* GSTOMX_DUMMY_H */
