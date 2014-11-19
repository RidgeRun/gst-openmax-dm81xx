/* GStreamer
 *
 * Copyright (C) 2009 Texas Instruments, Inc - http://www.ti.com/
 *
 * Description: OMX Camera element
 *  Created on: Aug 31, 2009
 *      Author: Rob Clark <rob@ti.com>
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
 */


#ifndef GSTOMX_CAMERA_H
#define GSTOMX_CAMERA_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_OMX_CAMERA(obj) (GstOmxCamera *) (obj)
#define GST_OMX_CAMERA_TYPE (gst_omx_camera_get_type ())

typedef struct GstOmxCamera GstOmxCamera;
typedef struct GstOmxCameraClass GstOmxCameraClass;

#include "gstomx_base_src.h"

struct GstOmxCamera
{
    GstOmxBaseSrc omx_base;
	/*TVP*/
	GOmxCore *tvp;

    /*< private >*/
    gint rowstride;     /**< rowstride of preview/video buffer */
#if 0
    GstClockTime duration;
    GstClockTime running_time;
    GstClockTime omx_delay;
#endif
    GOmxPort *port;
    gint alreadystarted;

	gint input_interface;
	gint fieldMerged;
	gint cap_mode;
	gint input_format;
	gint scan_type;
	gint override_colorspace;
	gint vif_mode;
};

struct GstOmxCameraClass
{
    GstOmxBaseSrcClass parent_class;
};

GType gst_omx_camera_get_type (void);

/* Default portstartnumber of Camera component */
#define OMX_CAMERA_DEFAULT_START_PORT_NUM 0

/* Define number of ports for differt domains */
#define OMX_CAMERA_PORT_VIDEO_NUM 1

/* Define start port number for differt domains */
#define OMX_CAMERA_PORT_VIDEO_START OMX_CAMERA_DEFAULT_START_PORT_NUM

/* Port index for camera component */
#define OMX_CAMERA_PORT_VIDEO_OUT_VIDEO         OMX_CAMERA_PORT_VIDEO_START
/* ************************************************************************* */

G_END_DECLS

#endif /* GSTOMX_CAMERA_H */
