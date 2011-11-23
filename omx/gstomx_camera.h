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

    /*< private >*/
    gint mode, next_mode;
    gint shutter;
    gint img_count;
#ifdef USE_OMXTICORE
    gint img_thumbnail_width;
    gint img_thumbnail_height;
    gint img_focusregion_width;
    gint img_focusregion_height;
    gint img_regioncenter_x;
    gint img_regioncenter_y;
    gdouble click_x;
    gdouble click_y;
#endif

    gint rowstride;     /**< rowstride of preview/video buffer */

    GOmxPort *vid_port;
    GOmxPort *img_port;
    GOmxPort *in_vid_port;
    GOmxPort *in_port;
    GOmxPort *msr_port;
#if 0
    GOmxPort *in_port;
#endif

    GstPad   *vidsrcpad;
    GstPad   *imgsrcpad;
    GstPad   *thumbsrcpad;

    /* if EOS is pending (atomic) */
    gint pending_eos;
};

struct GstOmxCameraClass
{
    GstOmxBaseSrcClass parent_class;
};

GType gst_omx_camera_get_type (void);


/* (can we get these port #'s in a cleaner way??) */
/* ****** from omx_iss_cam_def.h ******************************************* */
/* Default portstartnumber of Camera component */
#define OMX_CAMERA_DEFAULT_START_PORT_NUM 0

/* Define number of ports for differt domains */
#define OMX_CAMERA_PORT_OTHER_NUM 1
#define OMX_CAMERA_PORT_VIDEO_NUM 4
#define OMX_CAMERA_PORT_IMAGE_NUM 1
#define OMX_CAMERA_PORT_AUDIO_NUM 0

/* Define start port number for differt domains */
#define OMX_CAMERA_PORT_OTHER_START OMX_CAMERA_DEFAULT_START_PORT_NUM
#define OMX_CAMERA_PORT_VIDEO_START (OMX_CAMERA_PORT_OTHER_START + OMX_CAMERA_PORT_OTHER_NUM)
#define OMX_CAMERA_PORT_IMAGE_START (OMX_CAMERA_PORT_VIDEO_START + OMX_CAMERA_PORT_VIDEO_NUM)
#define OMX_CAMERA_PORT_AUDIO_START (OMX_CAMERA_PORT_IMAGE_START + OMX_CAMERA_PORT_IMAGE_NUM)

/* Port index for camera component */
#define OMX_CAMERA_PORT_OTHER_IN                (OMX_CAMERA_PORT_OTHER_START + 0)
#define OMX_CAMERA_PORT_VIDEO_IN_VIDEO          (OMX_CAMERA_PORT_VIDEO_START + 0)
#define OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW       (OMX_CAMERA_PORT_VIDEO_START + 1)
#define OMX_CAMERA_PORT_VIDEO_OUT_VIDEO         (OMX_CAMERA_PORT_VIDEO_START + 2)
#define OMX_CAMERA_PORT_VIDEO_OUT_MEASUREMENT   (OMX_CAMERA_PORT_VIDEO_START + 3)
#define OMX_CAMERA_PORT_IMAGE_OUT_IMAGE         (OMX_CAMERA_PORT_IMAGE_START + 0)
/* ************************************************************************* */


G_END_DECLS

#endif /* GSTOMX_CAMERA_H */
