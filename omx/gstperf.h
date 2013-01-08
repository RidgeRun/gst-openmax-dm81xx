/*
 * Copyright (C) 2011-2012 Texas Instruments Inc
 *
 * Author: Brijesh Singh <bksingh@ti.com>
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

#ifndef __GST_PERF_H__
#define __GST_PERF_H__

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating perf objects */
#define GST_TYPE_PERF \
  (gst_perf_get_type())
#define GST_PERF(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PERF,Gstperf))
#define GST_PERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PERF,GstperfClass))
#define GST_IS_PERF(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PERF))
#define GST_IS_PERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PERF))

typedef struct _Gstperf      Gstperf;
typedef struct _GstperfClass GstperfClass;

/* _Gstperf object */
struct _Gstperf
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* statistics */
  guint64 frames_count, last_frames_count, total_size;

  GstClockTime start_ts;
  GstClockTime last_ts;
  GstClockTime interval_ts;

  gboolean print_fps, print_arm_load, fps_update_interval;
  unsigned long int  total;
  unsigned long int  prevTotal;
  unsigned long int userTime;
  unsigned long int  prevuserTime;
};

/* _GstperfClass object */
struct _GstperfClass
{
  GstBaseTransformClass  parent_class;
};

/* External function enclarations */
GType gst_perf_get_type(void);

G_END_DECLS

#endif /* __GST_PERF_H__ */


